-- bzcapture.lua - record Battlezone game frames from MAME for bzcompare.
--
--   BZCAP_OUT=attract.bzcap BZCAP_FRAMES=2000 mame bzonea -autoboot_script tools/mame/bzcapture.lua
--
-- The set must be bzonea: revision 1 of the program, which the source
-- (historicalsource/battlezone) and the addresses below describe.
--
-- One FRAM record each time the main line starts a frame (MAIN's LSR SYNC
-- shifts a 1 out): RAM, vector RAM and the math box as they are then. A
-- frame of the port, started from one record, must end with the next one's
-- memory. Ahead of each FRAM, what the main line took in during the frame
-- before: the random numbers it read (RNDM) and its reads of the memory
-- the interrupt keeps and of the switches (IRDS). Format: src/capture/capture.h.
--
-- Environment:
--   BZCAP_OUT     output file (default bzcapture.bzcap)
--   BZCAP_FRAMES  stop MAME after this many frames (default: until MAME exits)
--   BZCAP_PLAY    a played script, see below
--   BZCAP_INFO    1: print the input fields and the math box's items, then go on

local SYNC   = 0x39     -- the interrupt counts frames due here; MAIN shifts it right
local VTIMER = 0xED     -- the interrupt's last write
local PRAND  = 0x182A   -- POKEY random
local NMIVEC = 0x7FFA

local out_path = os.getenv("BZCAP_OUT") or "bzcapture.bzcap"
local max_frames = tonumber(os.getenv("BZCAP_FRAMES") or "0") or 0

local machine = manager.machine
if machine.system.name ~= "bzonea" then
    emu.print_error("bzcapture: this is " .. machine.system.name ..
        ", the addresses are for bzonea; recording anyway")
end

local cpu = machine.devices[":maincpu"]
local space = cpu.spaces["program"]
local f = assert(io.open(out_path, "wb"))
f:write("BZCAP1\0\0")
f:write("ROMS", string.pack("<I4", 0x5000), space:read_range(0x3000, 0x7FFF, 8))

-- The math box's registers are inside its device: by its save state items.
local mb_reg, mb_result
do
    local mb = machine.devices[":mathbox"]
    for name, id in pairs(mb.items) do
        if os.getenv("BZCAP_INFO") then emu.print_info("bzcapture: mathbox item " .. name) end
        if name:find("m_reg") then mb_reg = emu.item(id) end
        if name:find("m_result") then mb_result = emu.item(id) end
    end
    if not mb_reg or not mb_result then emu.print_error("bzcapture: math box items not found") end
end

if os.getenv("BZCAP_INFO") then
    for tag, port in pairs(machine.ioport.ports) do
        for name, field in pairs(port.fields) do
            emu.print_info(string.format("bzcapture: port %s field '%s' mask %02X", tag, name, field.mask))
        end
    end
end

local nmi_count = 0
local frames = 0
local done = false
local FRAM_SIZE = 8 + 0x400 + 0x1000 + 34

taps = {}               -- keep the taps referenced: they are removed when collected
local quiet = false     -- the script's own reads go through the read taps too
local in_nmi = false

local function peek(a)
    quiet = true
    local v = space:read_u8(a)
    quiet = false
    return v
end

local function memory()
    quiet = true
    local d = space:read_range(0x0000, 0x03FF, 8) .. space:read_range(0x2000, 0x2FFF, 8)
    quiet = false
    local t = {}
    for i = 0, 15 do t[#t + 1] = string.pack("<I2", (mb_reg and mb_reg:read(i) or 0) & 0xFFFF) end
    t[#t + 1] = string.pack("<I2", (mb_result and mb_result:read(0) or 0) & 0xFFFF)
    return d .. table.concat(t)
end

-- A played script: BZCAP_PLAY is a list of "FRAME WHAT [ARGS]" separated by
-- ';', FRAME counted in game frames from power-on:
--   coin N | start N | fire N    close the switch for N frames
--   drive L R                    the two levers: f, b or - each (forward, back, middle)
--   auto N                       from here on: drive about at random and fire
--                                every N frames
--   aim N                        from here on: turn to the enemy, drive at him and
--                                fire every N frames when he is ahead
--   poke ADDR VAL                write a byte (hex) into the machine
--   dip PORT VAL                 set a bank of option switches (DSW0, DSW1) to VAL (hex)
local play = {}
for entry in (os.getenv("BZCAP_PLAY") or ""):gmatch("[^;]+") do
    local w = {}
    for word in entry:gmatch("%S+") do w[#w + 1] = word end
    if #w >= 2 then play[#play + 1] = { frame = tonumber(w[1]), what = w[2], a = w[3], b = w[4] } end
end
local ports = machine.ioport.ports
local function field(port, name)
    local p = ports[port]
    local fl = p and p.fields[name]
    if not fl then emu.print_error("bzcapture: no field '" .. name .. "' in " .. port) end
    return fl
end
local switches = {
    coin = field(":IN0", "Coin 1"), start = field(":IN3", "1 Player Start"), fire = field(":IN3", "P1 Button 1"),
}
local lever = {
    lf = field(":IN3", "P1 Left Stick/Up"), lb = field(":IN3", "P1 Left Stick/Down"),
    rf = field(":IN3", "P1 Right Stick/Up"), rb = field(":IN3", "P1 Right Stick/Down"),
}
local held = {}
local auto = 0
local aim = 0
local auto_left = 0
local seed = 12345

local function rnd(n)   -- the script's own, so that a recording can be made again
    seed = (seed * 1103515245 + 12345) & 0x7fffffff
    return (seed >> 8) % n
end

local function set(fl, on) if fl then fl:set_value(on and 1 or 0) end end

local function drive(l, r)
    set(lever.lf, l == "f"); set(lever.lb, l == "b")
    set(lever.rf, r == "f"); set(lever.rb, r == "b")
end

local function play_frame()
    for name, left in pairs(held) do
        if left <= 0 then
            set(switches[name], false)
            held[name] = nil
        else
            held[name] = left - 1
        end
    end
    for _, e in ipairs(play) do
        if e.frame == frames then
            if switches[e.what] then
                set(switches[e.what], true)
                held[e.what] = tonumber(e.a or "3")
            elseif e.what == "drive" then
                drive(e.a or "-", e.b or "-")
            elseif e.what == "auto" then
                auto = tonumber(e.a or "8")
                aim = 0
            elseif e.what == "aim" then
                aim = tonumber(e.a or "4")
                auto = 0
            elseif e.what == "dip" then
                -- dip PORT VALUE: an option switch bank as a whole (hex): DSW0 is 0A00, DSW1 0C00
                local port = ports[":" .. e.a]
                local v = tonumber(e.b, 16)
                if port then
                    -- (a field's value is given in place, not shifted down)
                    for _, fl in pairs(port.fields) do
                        fl.user_value = v & fl.mask
                    end
                end
            elseif e.what == "poke" then
                quiet = true
                space:write_u8(tonumber(e.a, 16), tonumber(e.b, 16))
                f:write("POKE", string.pack("<I4I2B", 3, tonumber(e.a, 16), tonumber(e.b, 16)))
                quiet = false
            end
        end
    end
    if aim > 0 then
        -- Turn to the enemy as the game's own "ENEMY TO LEFT/RIGHT" has it
        -- (PTURN: how far off, SAVE2: its sign), then drive at him and fire.
        local pturn, side = peek(0xD0), peek(0x16)
        if pturn > 2 then
            if side >= 0x80 then drive("f", "b") else drive("b", "f") end
        else
            drive("f", "f")
        end
        if pturn < 4 and frames % aim == 0 and not held.fire then
            set(switches.fire, true)
            held.fire = 2
        end
    end
    if auto > 0 then
        if auto_left <= 0 then
            local moves = { "f", "f", "f", "b", "-" }
            drive(moves[rnd(5) + 1], moves[rnd(5) + 1])
            auto_left = 5 + rnd(40)
        end
        auto_left = auto_left - 1
        if frames % auto == 0 and not held.fire then
            set(switches.fire, true)
            held.fire = 2
        end
    end
end

-- Inside the interrupt routine: from the CPU fetching the NMI vector until
-- the routine's last write (VTIMER: a DEC that leaves it above zero, or
-- the STA of a new 6 after the DEC reached zero).
-- The main line reads the vector's address as data now and then (its
-- CMP (0,X) "to waste time" goes wherever page 0 happens to point): a
-- fetch of the vector comes right after the CPU pushed the status byte,
-- within two cycles.
local last_push = -1.0
local CYCLE = 1.0 / 1512000.0
taps.stack = space:install_write_tap(0x0100, 0x01FF, "bzcap_stack",
    function(offset, data, mask)
        if not quiet then last_push = machine.time:as_double() end
    end)
taps.vector = space:install_read_tap(NMIVEC, NMIVEC, "bzcap_vector",
    function(offset, data, mask)
        if not quiet and machine.time:as_double() - last_push < 2.5 * CYCLE then
            in_nmi = true
            nmi_count = nmi_count + 1
        end
    end)
taps.vtimer = space:install_write_tap(VTIMER, VTIMER, "bzcap_vtimer",
    function(offset, data, mask)
        if quiet then return end
        in_nmi = (data & 0xFF) == 0
    end)

local rnds = {}
taps.prand = space:install_read_tap(PRAND, PRAND, "bzcap_prand",
    function(offset, data, mask)
        if not in_nmi and not quiet then rnds[#rnds + 1] = string.char(data & 0xFF) end
    end)

-- What the main line reads of the memory the interrupt keeps (credits, the
-- buffer handshake, frame sync, interrupt count, coin mode, the tune
-- pointers) and of the switches: address and value of every read, in
-- order. With these a frame can be run again without the interrupts and
-- comes out the same. (The same list is in src/capture/capture.c.)
local irds = {}
local BUFRDY = 0x28
local function ird(offset, data, mask)
    if in_nmi or quiet then return end
    -- (MAIN polls BUFRDY until the interrupt hands it a buffer: only the
    -- read that got one is kept)
    if offset == BUFRDY and (data & 0xFF) == 0 then return end
    -- (and polls SYNC at MAIN, 5000: LSR SYNC, BCC MAIN. Any other read of
    -- it is kept: the game's "CMP (0,X) to waste time" takes its address
    -- from wherever X points in page 0, SYNC included.)
    if offset == SYNC then
        local pc = cpu.state["PC"].value
        if pc >= 0x5000 and pc <= 0x5003 then return end
    end
    irds[#irds + 1] = string.pack("<I2B", offset, data & 0xFF)
end
local WATCH = { {0x0E, 0x0E}, {0x28, 0x28}, {0x39, 0x3A}, {0xEB, 0xEB}, {0xEF, 0xF2},
                {0x0800, 0x0800}, {0x0A00, 0x0A00}, {0x0C00, 0x0C00}, {0x1828, 0x1828} }
for i, r in ipairs(WATCH) do
    taps["ird" .. i] = space:install_read_tap(r[1], r[2], "bzcap_ird" .. i, ird)
end

taps.sync = space:install_write_tap(SYNC, SYNC, "bzcap_sync",
    function(offset, data, mask)
        if quiet or in_nmi or done then return end
        local old = peek(SYNC)                      -- the tap runs before the write
        if (old & 1) == 0 or (data & 0xFF) ~= (old >> 1) then return end   -- no frame due
        frames = frames + 1
        play_frame()
        local r = table.concat(rnds)
        rnds = {}
        f:write("RNDM", string.pack("<I4", #r), r)
        local d = table.concat(irds)
        irds = {}
        f:write("IRDS", string.pack("<I4", #d), d)
        f:write("TIME", string.pack("<I4", 8), string.pack("<d", machine.time:as_double()))
        f:write("FRAM", string.pack("<I4", FRAM_SIZE))
        f:write(string.pack("<I4I4", frames, nmi_count))
        f:write(memory())
        if max_frames > 0 and frames >= max_frames then
            done = true
            f:flush()
            machine:exit()
        end
    end)

stop_sub = emu.add_machine_stop_notifier(function()
    f:close()
    emu.print_info(string.format("bzcapture: %d frames written to %s", frames, out_path))
end)

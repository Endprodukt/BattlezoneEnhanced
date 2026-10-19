-- bzsound.lua - record what Battlezone writes to its sound hardware, for
-- comparing the port's sound with MAME's (tools/soundcheck.sh).
--
--   BZSND_OUT=writes.txt mame bzonea -autoboot_script tools/mame/bzsound.lua -wavwrite mame.wav
--
-- Every write to the POKEY (1820-182F) and to the sound latch (1840) as a
-- line "SECONDS ADDRESS VALUE" (decimal seconds of emulated time, hex).
-- Played on the port's model of the board at the same times, the same
-- writes must give the same sound as MAME's WAV.
--
-- The game is played so that every sound comes: a coin and start, then
-- the tank turns to the enemy, drives at him and fires; the option
-- switches put the missile at 5000 points and the score is set to 5000,
-- so that missiles come; and the eight tunes of the POKEY are started one
-- after the other (by putting into RAM what SNDON puts there) while the
-- game goes on.

local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local space = cpu.spaces["program"]
local f = assert(io.open(os.getenv("BZSND_OUT") or "bzsound.txt", "w"))

taps = {}
local quiet = false
local function log(offset, data)
    if quiet then return end
    f:write(string.format("%.9f %04X %02X\n", machine.time:as_double(), offset, data & 0xff))
end
taps.pokey = space:install_write_tap(0x1820, 0x182F, "bzsnd_pokey", function(offset, data) log(offset, data) end)


local ports = machine.ioport.ports
local function field(port, name) return ports[port] and ports[port].fields[name] end
local coin, start, fire = field(":IN0", "Coin 1"), field(":IN3", "1 Player Start"), field(":IN3", "P1 Button 1")
local lf, lb = field(":IN3", "P1 Left Stick/Up"), field(":IN3", "P1 Left Stick/Down")
local rf, rb = field(":IN3", "P1 Right Stick/Up"), field(":IN3", "P1 Right Stick/Down")
local function set(fl, on) if fl then fl:set_value(on and 1 or 0) end end
local function drive(l, r)
    set(lf, l == "f"); set(lb, l == "b"); set(rf, r == "f"); set(rb, r == "b")
end

local function peek(a)
    quiet = true
    local v = space:read_u8(a)
    quiet = false
    return v
end
local function poke(a, v)
    quiet = true
    space:write_u8(a, v)
    quiet = false
end

-- SNDON for tune `bit`, as BZSOUN does it: the four channels' pointers from PNTRS.
local POINT, FRAMES, COUNT, PNTRS = 0xEF, 0xF7, 0xFB, 0x7864
local function sndon(bit)
    local y = bit * 4 + 3
    for x = 3, 0, -1 do
        local v = peek(PNTRS + y)
        if v ~= 0 then
            poke(POINT + x, v); poke(FRAMES + x, 1); poke(COUNT + x, 1)
        end
        y = y - 1
    end
end

-- The option switches: missile at 5000 (english, 3 tanks, bonus at 15000).
for _, fl in pairs(ports[":DSW0"].fields) do fl.user_value = 0x11 & fl.mask end

local done = {}
local function once(name, t, now) if now >= t and not done[name] then done[name] = true; return true end end
local fire_until = 0

-- (run from the latch's tap: the interrupt writes it every time, 246 times a second;
-- MAME without video calls no frame callbacks)
local function tick()
    local t = machine.time:as_double()
    if once("coin", 3.0, t) then set(coin, true) end
    if once("coin up", 3.15, t) then set(coin, false) end
    if once("credit", 4.0, t) and peek(0x0E) == 0 then poke(0x0E, 1) end   -- (a credit, if the coin was not taken)
    if once("start", 5.0, t) then set(start, true) end
    if once("start up", 5.15, t) then set(start, false) end
    if once("check", 6.5, t) then emu.print_error(string.format("bzsnd: playing %s", peek(0xCE) >= 0x80 and "yes" or "no")) end
    if once("score", 6.0, t) then poke(0xB8, 0x05) end          -- 5000 points: missiles from now on
    for i = 0, 7 do
        if once("tune" .. i, 40.0 + 3.0 * i, t) then sndon(i) end
    end
    -- the missile's buzz, as R2D3CK and DRADAR set it (the game puts its
    -- volume back to 0 every frame without a missile: set again each time)
    if t >= 70.0 and t < 72.0 then
        space:write_u8(0x1824, 0xff); space:write_u8(0x1826, 0xfe)
        space:write_u8(0x1825, 0xaf); space:write_u8(0x1827, 0xaf)
    end
    if t > 6.0 then
        -- aim as the game's own "ENEMY TO LEFT/RIGHT" has it, and fire
        local pturn, side = peek(0xD0), peek(0x16)
        if pturn > 2 then
            if side >= 0x80 then drive("f", "b") else drive("b", "f") end
        else
            drive("f", "f")
        end
        if pturn < 4 and t > fire_until + 0.6 then
            set(fire, true)
            fire_until = t
        elseif t > fire_until + 0.15 then
            set(fire, false)
        end
    end
end

local ticking = false
taps.latch = space:install_write_tap(0x1840, 0x1840, "bzsnd_latch", function(offset, data)
    log(offset, data)
    if not ticking then
        ticking = true
        tick()
        ticking = false
    end
end)

stop_sub = emu.add_machine_stop_notifier(function() f:close() end)

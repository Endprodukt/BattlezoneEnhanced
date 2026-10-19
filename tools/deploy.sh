#!/bin/sh
# deploy.sh - copy the Pi build to the cabinet, put the new program in place
# and start it there, so that what is on the screen is the new build. A
# game that is running is ended first. When the game is left, the cabinet's
# menu comes back (package/run.sh). If another game has the board, the new
# build is only installed. Run from the project's folder, in Git
# Bash, after the Pi build (BUILD.md). Arguments go to the game.
K="-i $HOME/.ssh/id_ed25519_vectorpie -o ConnectTimeout=8 -o BatchMode=yes"
H=pi@192.168.0.122
D=/persistent/games/bzenhanced
ssh $K $H "mkdir -p $D/save $D/sounds" || exit 1
scp -q $K build/pi/battlezone $H:$D/battlezone.new || exit 1
scp -q $K package/game.ini package/battlezone.ini package/marquee.png package/run.sh $H:$D/ || exit 1
ssh $K $H "rm -f $D/sounds/*.wav" || exit 1     # (the pack only: the owner's own are in save/sounds)
scp -q $K package/sounds/*.wav $H:$D/sounds/ || exit 1
# Another game on the board (started from the cabinet's menu): install only,
# do not start ours over it (two programs on one board slow both down).
other=$(ssh $K $H 'for p in /proc/[0-9]*; do ls -l $p/fd 2>/dev/null | grep -q ttyACM && tr "\000" " " < $p/cmdline && echo; done' | grep -a -v -e vector_pie_menu -e battlezone)
if [ -n "$other" ]; then
    ssh $K $H "cd $D && chmod 755 battlezone.new run.sh && mv -f battlezone.new battlezone && md5sum battlezone && grep '^version' game.ini"
    echo "deploy.sh: installed, not started: another program has the board: $other"
    exit 0
fi
ssh $K $H "cd $D && pkill -INT -x battlezone; for i in 1 2 3 4 5 6; do pgrep -x battlezone > /dev/null || break; sleep 0.5; done; pkill -KILL -x battlezone; chmod 755 battlezone.new run.sh && mv -f battlezone.new battlezone && md5sum battlezone && grep '^version' game.ini && (setsid nohup ./run.sh $* > /dev/null 2>&1 &) ; sleep 6; pgrep -x battlezone > /dev/null && echo game-running || echo GAME-NOT-RUNNING; tail -n 5 run.log"
md5sum build/pi/battlezone

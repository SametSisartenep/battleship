# Battleship

An on-line multi-player implementation of Battleship (a.k.a. Sink the Fleet).

# How to play

Join a server

	% games/bts arcade.antares-labs.eu

Then wait for another player to show up (admire the velero).  When
they do, you'll have to place your ships in the lower (local) board by
clicking LMB when the ship is at the right location; you can also
change the orientation of the ship by pressing MMB.  If you are not
happy with the current layout, press RMB and select “relocate ships” to
start all over again (tip: if you don't want to move a ship, click LMB
without moving the mouse.) Once you are done—a banner at the bottom
will inform you—, press RMB and choose “done”.

At this moment the battle will begin.  Each of you has a turn (read
[battleship.pdf](battleship.pdf)), and if you are the one shooting you
have to aim at a tile at the upper (target) board—which will show a
reticle—and press LMB to fire a shot.  If you hit an enemy ship, the
tile will show an X, otherwise a O. If you are the one waiting, well,
just do that.

There's no turn timeout so the battles can get as long as any of the
players will.  If you quit or your connection is lost, your opponent
wins.

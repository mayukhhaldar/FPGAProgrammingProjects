# Inspiration of Slither.io

This is an embedded game that is to be played on an ARM based SoC (System-on-Chip).

To demo this game, copy and paste the script into https://cpulator.01xz.net/?sys=arm-de1soc.
- Hit Compile and Load and press Continue. 
- Make sure the language is in C. 

On the right column use the arrow keys and keyboard peripherals to control the game.

HOW THE GAME WORKS:
The user spawns in as a green snake in a field of enemies and food (pink dots). Eating food makes you bigger, you can eat other equally sized or smaller snakes only. If you try eatin a 
larger snake you will die. Your score is displayed on the HEX. When the game ends, if the LEDR light up you won, else you lost.
Hex will go to all 0 for a loss, all 1 for a win. Press Key 0 to play again. Press any other key to stop.
The objective is to be the last snake on the board. Food is spawned using interupts with the A9 timer every 3 seconds. Movement uses the PS2 keyboard.

MOVEMENT CONTROLS:
UP Arrow: Go up
DOWN Arrow: Go down
Left Arrow: Go left
Right Arrow: Go right

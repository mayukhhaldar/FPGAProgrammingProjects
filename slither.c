#include <stdbool.h>
#include <stdlib.h>

#define numOfCompSnakes 5
#define TIMER_BASE            0xFF202000

volatile int pixel_buffer_start; // global variable
void clear_screen();
void plot_pixel(int x, int y, short int line_color);
void wait_for_vsync();
void init_user();
void init_computer();
void draw_snake();
void update_player_position();
void update_computer_position();
void check_game_status();
void update_velocity();
void restart();
void update_computer_velocity();
void grow_player();
void init_board();
void init_food();
void draw_board();
void spawn_new_food();
void draw_4by4_box(int topLeftX, int topLeftY, short colour);
bool check_for_food_hit();
void check_for_computer_food_hit();
void check_for_food();
void check_for_snake();
void check_to_attack();
void init_snakeSpace();
void config_GIC(void);
void enable_A9_interrupts(void);
void disable_A9_interrupts(void);
void set_A9_IRQ_stack(void);
void __attribute__((interrupt)) __cs3_reset(void);
void __attribute__((interrupt)) __cs3_isr_undef(void);
void __attribute__((interrupt)) __cs3_isr_swi(void);
void __attribute__((interrupt)) __cs3_isr_pabort(void);
void __attribute__((interrupt)) __cs3_isr_dabort(void);
void __attribute__((interrupt)) __cs3_isr_fiq(void);
void config_timer();
void __attribute__((interrupt)) __cs3_isr_irq(void);
void config_interrupt(int N, int CPU_target);
void config_KEYs();
void config_interval_timer();
void update_score_to_hex();
void win_to_ledr();
bool play_again();
void result_to_hex(bool win);
//boxes comprise the body of the snake, each box is 4x4
struct box {
    //position of the top left corner of the 4x4 box
    int x;
    int y;
    //colour
    short colour;
};

struct snake {
    //dynamic array with all the players coordinates (has all the boxes that make up the body)
    struct box* body;
    //size of the snake
    int size;
    //velocities
    //at a given time the user may only move in 1 direction, i.e. no diagonal movement
    int dx;
    int dy;
    //unique identifier to determine which snake it is for collision, user gets numOfComps snakes, computer snakes get their array index
    int id;
    //boolean to determine if the player is still in the game
    bool alive;

};

void grow_computer(struct snake* slither);

//global short array 2-dim array to represent what spots on the "board" are occupied by food
//short values stored are simply colours of the food, all food is 0xF81F (pink)
//can later also be used to store location of enemies, or maybe use another seperate gameState like 2-dim array for that
short gameState[240][320];

//board that contains the locations of all snakes. If a snake occupies a pixel, their ID is written at that index. If unoccupied then -1 is at the index
int snakeSpace[240][320];

//global snake for the user
struct snake user;

//global snakes for computer
struct snake computer[numOfCompSnakes];

//global variables to chase food
int xFood[numOfCompSnakes];
int yFood[numOfCompSnakes];
bool found[numOfCompSnakes];

//global variables to chase user snake
bool snakeFound[numOfCompSnakes];
int xSnake[numOfCompSnakes];
int ySnake[numOfCompSnakes];

int seedVal;

int main(void)
{
    bool play = true;
    seedVal = 0;
    while (play) {
        seedVal++;
        srand(seedVal);
        //initialize the user snake, food and boards
        init_board();
        init_snakeSpace();
        init_food();
        init_user();
        init_computer();

        set_A9_IRQ_stack();
        config_GIC();
        config_interval_timer();
        enable_A9_interrupts();


        volatile int* pixel_ctrl_ptr = (int*)0xFF203020;
        // declare other variables(not shown)
        // initialize location and direction of rectangles(not shown)

        /* set front pixel buffer to start of FPGA On-chip memory */
        *(pixel_ctrl_ptr + 1) = 0xC8000000; // first store the address in the 
                                            // back buffer
        /* now, swap the front/back buffers, to set the front buffer location */
        wait_for_vsync();
        /* initialize a pointer to the pixel buffer, used by drawing functions */
        pixel_buffer_start = *pixel_ctrl_ptr;
        clear_screen(); // pixel_buffer_start points to the pixel buffer
        /* set back pixel buffer to start of SDRAM memory */
        *(pixel_ctrl_ptr + 1) = 0xC0000000;
        pixel_buffer_start = *(pixel_ctrl_ptr + 1); // we draw on the back buffer
        bool cont = true;
        bool win = true;
        while (cont)
        {
            check_game_status();

            update_score_to_hex();
            clear_screen();
            //drawing board draws all the food
            draw_board();
            //draws the user
            draw_snake();
            //updates the user velocity based on key input

            update_velocity();
            //updates the velocities of the computer
            update_computer_velocity();
            //update the player position based on velocity
            update_player_position();
            //update the computer positions based on velocity
            update_computer_position();
            //checking if the games over
            if (!user.alive) {
                cont = false;
                win = false;
            }
            bool computersRemain = false;
            for (int i = 0; i < numOfCompSnakes; i++) {
                if (computer[i].alive) computersRemain = true;

            }
            if (!computersRemain) cont = false;

            //check to see if it hit food and grow the snake accordingly
            check_for_food_hit();

            check_for_computer_food_hit();

            wait_for_vsync(); // swap front and back buffers on VGA vertical sync
            pixel_buffer_start = *(pixel_ctrl_ptr + 1); // new back buffer
        }
        if (win) win_to_ledr();
        result_to_hex(win);
        play = play_again();
    }
   
   
}

// code for subroutines 
void plot_pixel(int x, int y, short int line_color)
{
    *(short int*)(pixel_buffer_start + (y << 10) + (x << 1)) = line_color;
}

//write black to all pixels
void clear_screen() {
    for (int x = 0; x < 320; x++) {
        for (int y = 0; y < 240; y++) {
            plot_pixel(x, y, 0x0000);

        }

    }



}
//wait for synchronization
void wait_for_vsync() {
    volatile int* pixel_ctrl_ptr = 0xFF203020;

    register int status;

    *pixel_ctrl_ptr = 1;

    status = *(pixel_ctrl_ptr + 3);
    while ((status & 0x01) != 0) {
        status = *(pixel_ctrl_ptr + 3);
    }
    return;
}

//initialize the user to a random horizontal line, user starts off as a 4 length snake
void init_user() {
    user.body = malloc(4 * sizeof(struct box));
    int startX = rand() % 316;
    int startY = rand() % 236;
    //initialize the snake
    user.body[0].x = startX;
    user.body[0].y = startY;
    user.body[0].colour = 0x07E0;

    user.body[1].x = startX - 4;
    user.body[1].y = startY;
    user.body[1].colour = 0x07E0;

    user.body[2].x = startX - 8;
    user.body[2].y = startY;
    user.body[2].colour = 0x07E0;

    user.body[3].x = startX -12;
    user.body[3].y = startY;
    user.body[3].colour = 0x07E0;

    user.size = 4;
    user.dx = 1;
    user.dy = 0;
    user.alive = true;
    user.id = numOfCompSnakes;

    //loop to initialize the user onto the snakeSpace board
    for (int j = 0; j < user.size; j++) {
        int x =user.body[j].x;
        int y = user.body[j].y;
        for (int k = 0; k < 4; k++) {
            for (int m = 0; m < 4; m++) {
                snakeSpace[y + k][x + m] = user.id;
            }
            
        }
    }

}

void init_computer(){

    short colourPalette[6];

    colourPalette[0] = 0xF800; // red
    colourPalette[1] = 0x001F; // blue
    colourPalette[2] = 0xFFE0; // yellow
    colourPalette[3] = 0xF81F; // magenta
    colourPalette[4] = 0x07FF; // cyan
    colourPalette[5] = 0xFCA0; // orange

    for(int i = 0; i < numOfCompSnakes; i++){
        computer[i].body = malloc(4 * sizeof(struct box));

            found[i] = false;
            snakeFound[i] = false;
            
            int startX = (rand() % 308) + 12;
            int startY = (rand() % 228) + 12;

            short snakeColour = colourPalette[rand() % 6];

            int xOrY = rand() % 2;

            int velocity;
            int direction = rand() % 2;

            if(direction == 1){
                velocity = 1;
            } else {
                velocity = -1;
            }

            //initialize the snake
            computer[i].alive = true;
            computer[i].id = i;
            computer[i].body[0].x = startX;
            computer[i].body[0].y = startY;
            computer[i].body[0].colour = snakeColour;

            if(xOrY == 1){

                computer[i].body[1].x = startX + (4 * velocity);
                computer[i].body[1].y = startY;
                computer[i].body[1].colour = snakeColour;

                computer[i].body[2].x = startX + (8 * velocity);
                computer[i].body[2].y = startY;
                computer[i].body[2].colour = snakeColour;

                computer[i].body[3].x = startX + (12 * velocity);
                computer[i].body[3].y = startY;
                computer[i].body[3].colour = snakeColour;

                computer[i].size = 4;
                computer[i].dx = -1 * velocity;
                computer[i].dy = 0;

            } else {

                computer[i].body[1].x = startX;
                computer[i].body[1].y = startY + (4 * velocity);
                computer[i].body[1].colour = snakeColour;

                computer[i].body[2].x = startX;
                computer[i].body[2].y = startY + (8 * velocity);
                computer[i].body[2].colour = snakeColour;

                computer[i].body[3].x = startX;
                computer[i].body[3].y = startY + (12 * velocity);
                computer[i].body[3].colour = snakeColour;

                computer[i].size = 4;
                computer[i].dx = 0;
                computer[i].dy = -1 * velocity;
            }
            //loop to initialize the user onto the snakeSpace board
            for (int j = 0; j < computer[i].size; j++) {
                int x = computer[i].body[j].x;
                int y = computer[i].body[j].y;
                for (int k = 0; k < 4; k++) {
                    for (int m = 0; m < 4; m++) {
                        snakeSpace[y + k][x + m] = computer[i].id;
                    }

                }
            }
    }

}

//draws the user
//iterate over all the boxes that make up the user snake, draw the box
void draw_snake() {
    if (user.alive) {
        for (int i = 0; i < user.size; i++) {
            short colour = user.body[i].colour;
            int topLeftX = user.body[i].x;
            int topLeftY = user.body[i].y;

            //drawing a 4x4 box
            draw_4by4_box(topLeftX, topLeftY, colour);
        }
    }
    for(int i = 0; i < numOfCompSnakes; i++){
        if (!computer[i].alive) continue;
        for(int j = 0; j < computer[i].size; j++){
            short colour = computer[i].body[j].colour;
            int topLeftX = computer[i].body[j].x;
            int topLeftY = computer[i].body[j].y;

            draw_4by4_box(topLeftX, topLeftY, colour); 
        }
    }

    return;

}

/*
Logic: from the tail up until the box right behind the head,
the previous box assumes the position of the one in front of it
then draw the head in the direction of dx or dy from the last updated position
*/
/*
Logic: from the tail up until the box right behind the head,
the previous box assumes the position of the one in front of it
then draw the head in the direction of dx or dy from the last updated position
*/
void update_player_position() {
    if (!user.alive) return;
    bool goingRight = user.dx == 1;
    bool goingLeft = user.dx == -1;
    bool goingDown = user.dy == 1;
    bool goingUp = user.dy == -1;
    //first we wipe the user from the snakeSpace board before we move them so we still know where they are
    //loop to initialize the user onto the snakeSpace board
    for (int j = 0; j < user.size; j++) {
        int x = user.body[j].x;
        int y = user.body[j].y;
        for (int k = 0; k < 4; k++) {
            for (int m = 0; m < 4; m++) {
                snakeSpace[y + k][x + m] = -1;
            }

        }
    }



    for (int i = user.size -1; i > 0; i--) {
        user.body[i].x = user.body[i - 1].x;
        user.body[i].y = user.body[i - 1].y;
    
    }
    //now we update the head depending on where we are going
    if (goingRight) {
        user.body[0].x = user.body[1].x + 4;
        user.body[0].y = user.body[1].y;

        if (user.body[0].x > 319) user.body[0].x = 0;

    }
    else if (goingLeft) {
        user.body[0].x = user.body[1].x - 4;
        user.body[0].y = user.body[1].y;

        if (user.body[0].x < 0) user.body[0].x = 319;


    }
    else if (goingUp) {
        user.body[0].x = user.body[1].x;
        user.body[0].y = user.body[1].y - 4;

        if (user.body[0].y < 0) user.body[0].y = 239;


    }
    else if (goingDown) {
        user.body[0].x = user.body[1].x;
        user.body[0].y = user.body[1].y + 4;

        if (user.body[0].y > 239) user.body[0].y = 0;

    }

    //checking to see if the head has made contact with another player
    int x = user.body[0].x;
    int y = user.body[0].y;
    bool hit = false;
    int hitID = -1;
    for (int m = 0; m < 4; m++) {
        for (int k = 0; k < 4; k++) {
            if (y + m < 240 && x + k < 320) {
                if (snakeSpace[y + m][x + k] != -1) {
                    hit = true;
                    hitID = snakeSpace[y + m][x + k];
                }
            }
        }
    }
    //determining who gets to live and who dies
    if (hit && hitID != user.id) {
        int otherSize = computer[hitID].size;
        if (user.size < otherSize) {
            user.alive = false;
            for (int k = 0; k < user.size; k++) {
            grow_computer(&computer[hitID]);
            }
            return;
        }
        else {
            computer[hitID].alive = false;
            for (int k = 0; k < otherSize; k++) {
                grow_player();
            }
        }
    
    }
    //now we update the user on the board again
    for (int j = 0; j < user.size; j++) {
        int x = user.body[j].x;
        int y = user.body[j].y;
        for (int k = 0; k < 4; k++) {
            for (int m = 0; m < 4; m++) {
                snakeSpace[y + k][x + m] = user.id;
            }

        }
    }

    return;

}

void update_computer_position(){
    
    for (int i = 0; i < numOfCompSnakes; i++) {
        if (!computer[i].alive) continue;
        //first we wipe the player from the board
        for (int j = 0; j < computer[i].size; j++) {
            int x = computer[i].body[j].x;
            int y = computer[i].body[j].y;
            for (int k = 0; k < 4; k++) {
                for (int m = 0; m < 4; m++) {
                    snakeSpace[y + k][x + m] = -1;
                }

            }
        }


        bool goingRight = computer[i].dx == 1;
        bool goingLeft = computer[i].dx == -1;
        bool goingDown = computer[i].dy == 1;
        bool goingUp = computer[i].dy == -1;

        for (int j = computer[i].size - 1; j > 0; j--) {
            computer[i].body[j].x = computer[i].body[j - 1].x;
            computer[i].body[j].y = computer[i].body[j - 1].y;

        }

        if (goingRight) {

            computer[i].body[0].x = computer[i].body[1].x + 4;
            computer[i].body[0].y = computer[i].body[1].y;

            if (computer[i].body[0].x > 319) computer[i].body[0].x = 0;

        }
        else if (goingLeft) {
            computer[i].body[0].x = computer[i].body[1].x - 4;
            computer[i].body[0].y = computer[i].body[1].y;

            if (computer[i].body[0].x < 0) computer[i].body[0].x = 319;


        }
        else if (goingUp) {
            computer[i].body[0].x = computer[i].body[1].x;
            computer[i].body[0].y = computer[i].body[1].y - 4;

            if (computer[i].body[0].y < 0) computer[i].body[0].y = 239;


        }
        else if (goingDown) {
            computer[i].body[0].x = computer[i].body[1].x;
            computer[i].body[0].y = computer[i].body[1].y + 4;

            if (computer[i].body[0].y > 239) computer[i].body[0].y = 0;

        }
        //checking to see if the head has made contact with another player
        int x = computer[i].body[0].x;
        int y = computer[i].body[0].y;
        bool hit = false;
        int hitID = -1;
        for (int m = 0; m < 4; m++) {
            for (int k = 0; k < 4; k++) {
                if (y + m < 240 && x + k < 320) {
                    if (snakeSpace[y + m][x + k] != -1) {
                        hit = true;
                        hitID = snakeSpace[y + m][x + k];
                    }
                }
            }
        }
        //determining who gets to live and who dies
        if (hit && hitID != computer[i].id) {
            if (hitID != numOfCompSnakes) {
                int otherSize = computer[hitID].size;
                if (computer[i].size < otherSize) {
                    computer[i].alive = false;
                    for (int k = 0; k < computer[i].size; k++) {
                        grow_computer(&computer[hitID]);
                    }
                }
                else {
                    computer[hitID].alive = false;
                    for (int k = 0; k < otherSize; k++) {
                            grow_computer(&computer[i]);
                    }
                }
            }
            else {
                int otherSize = user.size;
                if (computer[i].size < otherSize) {
                    computer[i].alive = false;
                    for (int k = 0; k < computer[i].size; k++) {
                        grow_player();
                    }
                }
                else {
                    user.alive = false;
                    for (int k = 0; k < otherSize; k++) {
                        grow_computer(&computer[i]);
                    }
                }



            }

        }

        if (computer[i].alive){
            //now we update the player on the board again
            for (int j = 0; j < computer[i].size; j++) {
                int x = computer[i].body[j].x;
                int y = computer[i].body[j].y;
                for (int k = 0; k < 4; k++) {
                    for (int m = 0; m < 4; m++) {
                        snakeSpace[y + k][x + m] = computer[i].id;
                    }

                }
            }
        }
    }
    

    return;
}

/*
Key 3: Left
Key 2: Up
Key 1: Down
Key 0: Right
Can not go right if you were just going left before
Can not go down if you were just going up before etc.
Player is always moving
*/

void check_game_status() {
    if (!user.alive) return;
    //point to the edge capture
    volatile int* edge_capture = (int*)0xFF20005C;
    //key 0
    if (*edge_capture == 1){
        restart();
    }
    //clear the edge capture
    *edge_capture = *edge_capture;

    return;

}

void restart(){
    
    //initialize the user snake, food and boards
    init_board();
    init_snakeSpace();
    init_food();
    init_user();
    init_computer();
    clear_screen();
}

void update_velocity(){
    if (!user.alive) return;
    
    volatile int* PS2_ptr = (int*)0xFF200100;
	int PS2_data, RVALID;
	char byte1 = 0, byte2 = 0, byte3 = 0;

	// PS/2 mouse needs to be reset (must be already plugged in)
	*(PS2_ptr) = 0xFF; // reset

	while (true) {
		PS2_data = *(PS2_ptr); // read the Data register in the PS/2 port
		RVALID = PS2_data & 0x8000; // extract the RVALID field
		if (RVALID) {
			/* shift the next data byte into the display */
			byte1 = byte2;
			byte2 = byte3;
			byte3 = PS2_data & 0xFF;
			
            //up arrow
            if(byte1 == 0xE0 && byte2 == 0xF0 && byte3 == 0x75 && user.dy != 1){
                user.dy = -1;
                user.dx = 0;
            }
            //left arrow
            if(byte1 == 0xE0 && byte2 == 0xF0 && byte3 == 0x6B && user.dx != 1){
                user.dx = -1;
                user.dy = 0;
            }
            //down arrow
            if(byte1 == 0xE0 && byte2 == 0xF0 && byte3 == 0x72 && user.dy != -1){
                user.dy = 1;
                user.dx = 0;
            }
            //right arrow
            if(byte1 == 0xE0 && byte2 == 0xF0 && byte3 == 0x74 && user.dx != -1){
                user.dx = 1;
                user.dy = 0;
            }

			if ((byte2 == (char)0xAA) && (byte3 == (char)0x00))
				// mouse inserted; initialize sending of data
				*(PS2_ptr) = 0xF4;
		} else {
            break;
        }
	}
}

void update_computer_velocity(){

    for(int i = 0; i < numOfCompSnakes; i++){
        if (!computer[i].alive) continue;
        check_for_food();
        check_for_snake();

        //logic for snake chasing, this has a higher priority over food
        if(snakeFound[i]){
            found[i] = false;

            if((computer[i].body[0].x == xSnake[i]) && (computer[i].body[0].y == ySnake[i])){
                snakeFound[i] = false;
                return;
            }

            if(computer[i].dy == 1){
                if(computer[i].body[0].y >= ySnake[i]){
                    if(computer[i].body[0].x < xSnake[i]){
                        computer[i].dx = 1;
                        computer[i].dy = 0;
                    } else {
                        computer[i].dx = -1;
                        computer[i].dy = 0;
                    }
                }
            } else if (computer[i].dy == -1){
                if(computer[i].body[0].y <= ySnake[i]){
                    if(computer[i].body[0].x < xSnake[i]){
                        computer[i].dx = 1;
                        computer[i].dy = 0;
                    } else {
                        computer[i].dx = -1;
                        computer[i].dy = 0;
                    }
                }                
            } else if (computer[i].dx == 1){
                if(computer[i].body[0].x >= xSnake[i]){
                    if(computer[i].body[0].y < ySnake[i]){
                        computer[i].dx = 0;
                        computer[i].dy = 1;
                    } else {
                        computer[i].dx = 0;
                        computer[i].dy = -1;
                    }
                }
            } else if (computer[i].dx == -1){
                if(computer[i].body[0].x <= xSnake[i]){
                    if(computer[i].body[0].y < ySnake[i]){
                        computer[i].dx = 0;
                        computer[i].dy = 1;
                    } else {
                        computer[i].dx = 0;
                        computer[i].dy = -1;
                    }
                }
            }

        //logic for food chasing
        } else if(found[i]){

            if((computer[i].body[0].x == xFood[i]) && (computer[i].body[0].y == yFood[i])){
                return;
            }

            if(computer[i].dy == 1){
                if(computer[i].body[0].y >= yFood[i]){
                    if(computer[i].body[0].x < xFood[i]){
                        computer[i].dx = 1;
                        computer[i].dy = 0;
                    } else {
                        computer[i].dx = -1;
                        computer[i].dy = 0;
                    }
                }
            } else if (computer[i].dy == -1){
                if(computer[i].body[0].y <= yFood[i]){
                    if(computer[i].body[0].x < xFood[i]){
                        computer[i].dx = 1;
                        computer[i].dy = 0;
                    } else {
                        computer[i].dx = -1;
                        computer[i].dy = 0;
                    }
                }                
            } else if (computer[i].dx == 1){
                if(computer[i].body[0].x >= xFood[i]){
                    if(computer[i].body[0].y < yFood[i]){
                        computer[i].dx = 0;
                        computer[i].dy = 1;
                    } else {
                        computer[i].dx = 0;
                        computer[i].dy = -1;
                    }
                }
            } else if (computer[i].dx == -1){
                if(computer[i].body[0].x <= xFood[i]){
                    if(computer[i].body[0].y < yFood[i]){
                        computer[i].dx = 0;
                        computer[i].dy = 1;
                    } else {
                        computer[i].dx = 0;
                        computer[i].dy = -1;
                    }
                }
            }

        } else {

            int newVelocity = rand() % 15;


            if (newVelocity == 0 && computer[i].dx != -1) {
                computer[i].dx = 1;
                computer[i].dy = 0;
            
            }
            
            if (newVelocity == 1 && computer[i].dy != -1) {
                computer[i].dy = 1;
                computer[i].dx = 0;
            }
        
            if (newVelocity == 2 && computer[i].dy != 1) {
                computer[i].dy = -1;
                computer[i].dx = 0;
            }
            
            if (newVelocity == 3 && computer[i].dx != 1) {
                computer[i].dx = -1;
                computer[i].dy = 0;

            }

        }
    }
        
}
/*
Logic: the new tail goes at the end of the list
its placement will be in line with the last two boxes that form the snake for continuity
*/
void grow_player() {
    user.size += 1;
    user.body = realloc(user.body,user.size * sizeof(struct box));
    user.body[user.size - 1].colour = 0x07E0;
    
    
    int tailX = user.body[user.size - 2].x;
    int tailY = user.body[user.size - 2].y;

    int tailX2 = user.body[user.size - 3].x;
    int tailY2 = user.body[user.size - 3].y;

    //getting the direction of the tail in moving reverse from 3rd last to 2nd last box
    bool addToRight = (tailY2 == tailY) && (tailX2 < tailX);
    bool addToLeft = (tailY2 == tailY) && (tailX2 > tailX);
    bool addToTop = (tailX2 == tailX) && (tailY2 > tailY);
    bool addToBottom = (tailX2 == tailX) && (tailY2 < tailY);
    if (addToRight) {
        user.body[user.size - 1].x = tailX + 4;
        user.body[user.size - 1].y = tailY;
    }
    else if(addToLeft) {
        user.body[user.size - 1].x = tailX - 4;
        user.body[user.size - 1].y = tailY;
    }
    else if (addToTop) {
        user.body[user.size - 1].x = tailX;
        user.body[user.size - 1].y = tailY - 4;
    }
    else if (addToBottom) {
        user.body[user.size - 1].x = tailX;
        user.body[user.size - 1].y = tailY + 4;
    }
    return;
    
}

void grow_computer(struct snake* slither){

    slither->size += 1;
    slither->body = realloc(slither->body, slither->size * sizeof(struct box));
    slither->body[slither->size - 1].colour = slither->body[0].colour;
    
    
    int tailX = slither->body[slither->size - 2].x;
    int tailY = slither->body[slither->size - 2].y;

    int tailX2 = slither->body[slither->size - 3].x;
    int tailY2 = slither->body[slither->size - 3].y;

    //getting the direction of the tail in moving reverse from 3rd last to 2nd last box
    bool addToRight = (tailY2 == tailY) && (tailX2 < tailX);
    bool addToLeft = (tailY2 == tailY) && (tailX2 > tailX);
    bool addToTop = (tailX2 == tailX) && (tailY2 > tailY);
    bool addToBottom = (tailX2 == tailX) && (tailY2 < tailY);
    if (addToRight) {
        slither->body[slither->size - 1].x = tailX + 4;
        slither->body[slither->size - 1].y = tailY;
    }
    else if(addToLeft) {
        slither->body[slither->size - 1].x = tailX - 4;
        slither->body[slither->size - 1].y = tailY;
    }
    else if (addToTop) {
        slither->body[slither->size - 1].x = tailX;
        slither->body[slither->size - 1].y = tailY - 4;
    }
    else if (addToBottom) {
        slither->body[slither->size - 1].x = tailX;
        slither->body[slither->size - 1].y = tailY + 4;
    }
    return;
}



//draws a 4x4 box given the coordinates of the top left corner of the box
void draw_4by4_box(int topLeftX, int topLeftY, short colour) {
    //drawing a 4x4 box
    plot_pixel(topLeftX, topLeftY, colour);
    plot_pixel(topLeftX + 1, topLeftY, colour);
    plot_pixel(topLeftX + 2, topLeftY, colour);
    plot_pixel(topLeftX + 3, topLeftY, colour);

    plot_pixel(topLeftX, topLeftY + 1, colour);
    plot_pixel(topLeftX + 1, topLeftY + 1, colour);
    plot_pixel(topLeftX + 2, topLeftY + 1, colour);
    plot_pixel(topLeftX + 3, topLeftY + 1, colour);

    plot_pixel(topLeftX, topLeftY + 2, colour);
    plot_pixel(topLeftX + 1, topLeftY + 2, colour);
    plot_pixel(topLeftX + 2, topLeftY + 2, colour);
    plot_pixel(topLeftX + 3, topLeftY + 2, colour);

    plot_pixel(topLeftX, topLeftY + 3, colour);
    plot_pixel(topLeftX + 1, topLeftY + 3, colour);
    plot_pixel(topLeftX + 2, topLeftY + 3, colour);
    plot_pixel(topLeftX + 3, topLeftY + 3, colour);
    return;
}

//initialize the board to all black
void init_board() {
    for (int i = 0; i < 240; i++) {
        for (int j = 0; j < 320; j++) {
        
            gameState[i][j] = 0x0000;
        }
    
    }


}
//generate 25 randomly placed food items to begin the game
void init_food() {
    for (int i = 0; i < 25; i++) {
        int x = rand() % 318;
        int y = rand() % 238;

        short colour = 0xF81F;
        gameState[y][x] = colour;
        

    
    }
    return;
}

//draws all the food
void draw_board() {
    for (int y = 0; y < 240; y++) {
        for (int x = 0; x < 320; x++) {

            short colour = gameState[y][x];
            plot_pixel(x, y, colour);
        }

    }
    return;

}

//iterate over the entire head of the snake
//if it collides with food, erase that food and grow the snake
bool check_for_food_hit() {
    if (!user.alive) return false;
    int x = user.body[0].x;
    int y =user.body[0].y;
    bool hit = false;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (y+i < 240 && x+j < 320) {
                if (gameState[y+i][x+j] != 0x000) {
                    gameState[y+i][x+j] = 0x0000;
                    hit = true;
                }
            }
        }
    }
    
    if (hit) grow_player();
    return hit;
}

void check_for_computer_food_hit(){
    
    for(int k = 0; k < numOfCompSnakes; k++){
        if (!computer[k].alive) continue;

        int x = computer[k].body[0].x;
        int y = computer[k].body[0].y;
        bool hit = false;
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                if (y+i < 240 && x+j < 320) {
                    if (gameState[y+i][x+j] != 0x000) {
                        gameState[y+i][x+j] = 0x0000;
                        found[k] = false;
                        hit = true;
                    }
                }
            }
        }       

        if(hit) grow_computer(&computer[k]);
    }
}

//spawns a signle food item in a random location
void spawn_new_food() {
    int x = rand() % 318;
    int y = rand() % 238;

    short colour = 0xF81F;
    gameState[y][x] = colour;
}

//logic to check if food is nearby
void check_for_food(){

    for(int k = 0; k < numOfCompSnakes; k++){
        if (!computer[k].alive) continue;

        if(!found[k]){
            int x = computer[k].body[0].x;
            int y = computer[k].body[0].y;

            for (int i = -8 ; i < 8; i++) {
                for (int j = -8; j < 8; j++) {
                    if (y+i < 240 && x+j < 320) {
                        if (gameState[y+i][x+j] != 0x000) {
                            if(snakeSpace[y+i][x+j] == -1){
                                found[k] = true;
                                xFood[k] = x + j;
                                yFood[k] = y + i;
                            }
                        }
                    }
                }
            }  
        }
    }
        
    return;
}

//logic to check if snake is nearby
void check_for_snake(){

        for(int k = 0; k < numOfCompSnakes; k++){
        if (!computer[k].alive) continue;

        if(!snakeFound[k]){
            int x = computer[k].body[0].x;
            int y = computer[k].body[0].y;

            for (int i = -20 ; i < 20; i++) {
                for (int j = -20; j < 20; j++) {
                    if (y+i < 240 && x+j < 320) {
                        if (gameState[y+i][x+j] != 0x000) {
                            if(snakeSpace[y+i][x+j] == numOfCompSnakes){
                                snakeFound[k] = true;
                                ySnake[k] = y+i;
                                xSnake[k] = x+j;
                            }
                            
                        }
                    }
                }
            }  
        }
    }

    check_to_attack();

}

//checks if it should attack
//it won't attack if user is bigger or out of range
//also if the snake has a new location, the chasing point will be updated
void check_to_attack(){
        
        bool snakeOutOfRange = true;

        for(int k = 0; k < numOfCompSnakes; k++){
            if (!computer[k].alive) continue;

            if(user.size > computer[k].size){
                snakeFound[k] = false;
            }

            if(snakeFound[k]){
                int x = computer[k].body[0].x;
                int y = computer[k].body[0].y;

                for (int i = -20 ; i < 20; i++) {
                    for (int j = -20; j < 20; j++) {
                        if (y+i < 240 && x+j < 320) {
                            if (gameState[y+i][x+j] != 0x000) {
                                if(snakeSpace[y+i][x+j] == numOfCompSnakes){
                                    snakeOutOfRange = false;
                                    ySnake[k] = y+i;
                                    xSnake[k] = x+j;
                                }
                            }
                        }
                    }
                }  
            }

            if(snakeOutOfRange) snakeFound[k] = false;
    }
}

//initialize the snakespace with all -1 to show unoccupied
void init_snakeSpace() {
    for (int i = 0; i < 240; i++) {
        for (int j = 0; j < 320; j++) {
            snakeSpace[i][j] = -1;   
        }
    
    }

}

/*
HIT DETECTION LOGIC:
-Have a 2dim array the size of the vga display memory. 320x240 to hold all the snakes location by using their ID
- Initialize with all -1, then with snakes on top of it
- every snake gets an ID. computer snakes get an ID that is their array index, user gets numComputerSnakes as its ID
-Every time before I update a users position I wipe them from the array using current location, wipe over with -1
- I calculate new position
- I check for hit detection
-In hit detection I check whos bigger and grow them, then set the alive flag of the other one to false
- If theyre still alive I update their new position on the board

*/


/*
* Initialize the banked stack pointer register for IRQ mode
*/
void set_A9_IRQ_stack(void) {
    int stack, mode;
    stack = 0xFFFFFFFF - 7; // top of A9 onchip memory, aligned to 8 bytes
    /* change processor to IRQ mode with interrupts disabled */
    mode = 0b11010010;
    asm("msr cpsr, %[ps]" : : [ps] "r"(mode));
    /* set banked stack pointer */
    asm("mov sp, %[ps]" : : [ps] "r"(stack));
    /* go back to SVC mode before executing subroutine return! */
    mode = 0b11010011;
    asm("msr cpsr, %[ps]" : : [ps] "r"(mode));
}

/*
* Turn on interrupts in the ARM processor
*/
void enable_A9_interrupts(void) {
    int status = 0b01010011;
    asm("msr cpsr, %[ps]" : : [ps] "r"(status));
}
/*
* Configure the Generic Interrupt Controller (GIC)
*/
void config_GIC(void) {
    //config_interrupt(73, 1); // configure the timer to interupt
    // Set Interrupt Priority Mask Register (ICCPMR). Enable interrupts of all
    // priorities
    /* configure the FPGA interval timer and KEYs interrupts */
    *((int*)0xFFFED848) = 0x00000101;
    

    *((int*)0xFFFEC104) = 0xFFFF;
    // Set CPU Interface Control Register (ICCICR). Enable signaling of
    // interrupts
    *((int*)0xFFFEC100) = 1;
    // Configure the Distributor Control Register (ICDDCR) to send pending
    // interrupts to CPUs
    *((int*)0xFFFED000) = 1;
}

// Define the remaining exception handlers
void __attribute__((interrupt)) __cs3_reset(void) {
    while (1);
}
void __attribute__((interrupt)) __cs3_isr_undef(void) {
    while (1);
}
void __attribute__((interrupt)) __cs3_isr_swi(void) {
    while (1);
}
void __attribute__((interrupt)) __cs3_isr_pabort(void) {
    while (1);
}
void __attribute__((interrupt)) __cs3_isr_dabort(void) {
    while (1);
}
void __attribute__((interrupt)) __cs3_isr_fiq(void) {
    while (1);
}

//IRQ
void __attribute__((interrupt)) __cs3_isr_irq(void) {
    // Read the ICCIAR from the CPU Interface in the GIC
    int interrupt_ID = *((int*)0xFFFEC10C);
    if (interrupt_ID == 72) { // check if interrupt is from the timer
        
        spawn_new_food();
        //write to status
        volatile int* status = (int*)  0xFF202000;
        *status = 0x1;
    }
    else
        while (1); // if unexpected, then stay here
        // Write to the End of Interrupt Register (ICCEOIR)
    *((int*)0xFFFEC110) = interrupt_ID;
}

void config_timer()
{
    volatile int* timer = (int *)0xFFFEC600;
    //set to count for 5 seconds
    *timer = 1000000000;
    //set IAE bits of timer
    *(timer + 2) = 0x7;


}

/*
* Configure Set Enable Registers (ICDISERn) and Interrupt Processor Target
* Registers (ICDIPTRn). The default (reset) values are used for other registers
* in the GIC.
*/
void config_interrupt(int N, int CPU_target) {
    int reg_offset, index, value, address;
    /* Configure the Interrupt Set-Enable Registers (ICDISERn).
    * reg_offset = (integer_div(N / 32) * 4
    * value = 1 << (N mod 32) */
    reg_offset = (N >> 3) & 0xFFFFFFFC;
    index = N & 0x1F;
    value = 0x1 << index;
    address = 0xFFFED100 + reg_offset;
    /* Now that we know the register address and value, set the appropriate bit */
    *(int*)address |= value;
    /* Configure the Interrupt Processor Targets Register (ICDIPTRn)
    * reg_offset = integer_div(N / 4) * 4
    * index = N mod 4 */
    reg_offset = (N & 0xFFFFFFFC);
    index = N & 0x3;
    address = 0xFFFED800 + reg_offset + index;
    /* Now that we know the register address and value, write to (only) the
    * appropriate byte */
    *(char*)address = (char)CPU_target;
}

/*
* Turn off interrupts in the ARM processor
*/
void disable_A9_interrupts(void) {
    int status = 0b11010011;
    asm("msr cpsr, %[ps]" : : [ps] "r"(status));
}

/* setup the interval timer interrupts in the FPGA */
void config_interval_timer()
{
    volatile int* interval_timer_ptr =
        (int*)TIMER_BASE; // interal timer base address
        /* set the interval timer period for scrolling the HEX displays */
    int counter = 300000000; // 1/(100 MHz) x 300x10^6 = 3 sec
    *(interval_timer_ptr + 0x2) = (counter & 0xFFFF);
    *(interval_timer_ptr + 0x3) = (counter >> 16) & 0xFFFF;
    /* start interval timer, enable its interrupts */
    *(interval_timer_ptr + 1) = 0x7; // STOP = 0, START = 1, CONT = 1, ITO = 1
}

void update_score_to_hex() {
    int score = user.size;
    int scoreTens = score / 10;
    int scoreOnes = score - scoreTens*10;
    volatile int* hex = (int*) 0xFF200020;
    int hexCodes[] = { 0b00111111, 0b00000110, 0b01011011, 0b01001111, 0b01100110,
               0b01101101, 0b01111101, 0b00000111, 0b01111111, 0b01100111 };
    scoreTens = hexCodes[scoreTens];
    scoreOnes = hexCodes[scoreOnes];

    int scoreCode = (scoreTens << 8) | scoreOnes;
    *hex = scoreCode;
    return;


}

void win_to_ledr() {
    volatile int* led = (int*)0xFF200000;
    int win = 0xFF;
    *led = win;
    return;
}
//key 0 to play again, anything else to stop
bool play_again() {
    while (true) {
        volatile int *edge = (int*)0xFF20005C;
        if (*edge == 1) return true;
        else if (*edge > 1) return false;
    }


}

void result_to_hex(bool win) {
    volatile int* hex = (int*)0xFF200020;
    int winCode = (0b00000110 << 24) | (0b00000110 << 16) | (0b00000110 << 8) | (0b00000110);
    int lossCode = (0b00111111 << 24) | (0b00111111 << 16) | (0b00111111 << 8) | (0b00111111);
    if (win)  *hex = winCode;
    else if (!win) *hex = lossCode;

    return;
}

/*
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
*/
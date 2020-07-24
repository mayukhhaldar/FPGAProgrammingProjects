#include <math.h>
#include <stdbool.h>

#define RECTXSIZE 12
#define RECTYSIZE 8

void plot_pixel(int x, int y, short int line_color);
void clear_screen();
void swap(int* x, int* y);
void wait_for_vsync();
void draw_line(int x0, int y0, int x1, int y1, short int line_color);
void initialize_drawing();
void update_positions();
void bound_checks();
void draw();
void draw_rectangle(int x, int y, int xSize, int ySize, short int colour);

volatile int* pixelControlPtr;
volatile int pixelBufferStart; // global variable

short int black = 0x00000;
short int blue = 0x001F;
short int white = 0xFFFFFF;


short int rectColourPicker[10];
int rectXPosition[8];
int rectYPosition[8];
int rectXVelocity[8];
int rectYVelocity[8];
short int rectColours[8];

int main(void)
{
    
    //get the default on-chip memory address from front buffer and clear the on-chip memory pixels
    pixelControlPtr = (int *)0xFF203020;
    pixelBufferStart = *pixelControlPtr;
    clear_screen();

    //set the backbuffer register to the address of the SDRAM
    *(pixelControlPtr + 1) = 0xC0000000;
    //we now will use the back buffer to draw, so set the address variable to the address in the back buffer
    pixelBufferStart = *(pixelControlPtr + 1);

    //intialize the rectangle positions and velocities with randomly generated values
    initialize_drawing();
    
    while (true){
        
        //clear the back buffer screen
        clear_screen();
        //update positions of the rectangles depending on velocities
        update_positions();
        //check if they hit the edge of the screen
        bound_checks();
        //draw the actual image pixels on the memory
        draw();

        //wait for DMA to read from memory and write to VGA and
        wait_for_vsync(); // swap front and back buffers on VGA vertical sync
        pixelBufferStart = *(pixelControlPtr + 1); // new back buffer address to be used to draw
    }
}

void initialize_drawing(){
    //fill the colour picker
    rectColourPicker[0] = 0x0000FB;  
    rectColourPicker[1] = 0x00C938;  
    rectColourPicker[2] = 0x00C9CAB;
    rectColourPicker[3] = 0xFBC9CA;
    rectColourPicker[4] = 0xD006CA;
    rectColourPicker[5] = 0xD00609;
    rectColourPicker[6] = 0x5CAFFF;
    rectColourPicker[7] = 0x5CE444;
    rectColourPicker[8] = 0xFFE444;
    rectColourPicker[9] = 0xFFFFFF;

    //set the x,y positions and velocities randomly, as well as the colours
    for(int i = 0; i < 8; i++){
        rectXPosition[i] = rand() % (320 - RECTXSIZE);
        rectYPosition[i] = rand() % (240 - RECTYSIZE);
        rectXVelocity[i] = rand() % 6 - 3;
        rectYVelocity[i] = rand() % 6 - 3;
        rectColours[i] = rectColourPicker[rand () % 9];
    }
}

//update the positions by adding the velocites to their respective positions
void update_positions(){

    for(int i = 0; i < 8; i++){
        rectXPosition[i] += rectXVelocity[i];
        rectYPosition[i] += rectYVelocity[i];
    }

}

//checks if the rectangles hit the wall and then adjusts accordingly
void bound_checks(){

    for(int i = 0; i < 8; i++){
        
        if(rectXPosition[i] < 0){
            rectXVelocity[i] = -rectXVelocity[i];
            rectXPosition[i] = 0;
        }

        if(rectYPosition[i] < 0){
            rectYVelocity[i] = -rectYVelocity[i];
            rectYPosition[i] = 0;
        }

        if(rectXPosition[i] > 319 - RECTXSIZE){
            rectXVelocity[i] = -rectXVelocity[i];
            rectXPosition[i] = 319 - RECTXSIZE;
        }

        if(rectYPosition[i] > 239 - RECTYSIZE){
            rectYVelocity[i] = -rectYVelocity[i];
            rectYPosition[i] = 239 - RECTYSIZE;
        }

    }

}

void draw(){

    //finding the middle of the rectangle
    int rectXMiddle = RECTXSIZE / 2;
    int rectYMiddle = RECTYSIZE / 2;

    for(int i = 0; i < 8; i++){

        //draws the rectangle
        draw_rectangle(rectXPosition[i], rectYPosition[i], RECTXSIZE, RECTYSIZE, rectColours[i]);

        //computes the points of where the line is to be drawn
        int lineXStart = rectXPosition[i] + rectXMiddle;
        int lineYStart = rectYPosition[i] + rectYMiddle;
        int lineXEnd, lineYEnd;
        //set the ends of the lines depending on which box it is
        if(i == 7){
            lineXEnd = rectXPosition[0] + rectXMiddle;
            lineYEnd = rectYPosition[0] + rectYMiddle;
        } else {
            lineXEnd = rectXPosition[i + 1] + rectXMiddle;
            lineYEnd = rectYPosition[i + 1] + rectYMiddle;
        }

        //draw the lines
        draw_line(lineXStart, lineYStart, lineXEnd, lineYEnd, white);
    }
}

//creates the rectangle boxes
void draw_rectangle(int x, int y, int xSize, int ySize, short int colour){
	for (int i = y; i < y + ySize; i++) {
		draw_line(x, i, x + xSize - 1, i, colour);
	}
}

void clear_screen(){
    for(int x = 0; x < 320; x++){
        for(int y = 0; y < 240; y++){
            plot_pixel(x, y, black);
        }
    }
}

void swap(int *x, int *y) {
	int temp = *x;
	*x = *y;
	*y = temp;
}


void draw_line(int x0, int y0, int x1, int y1, short int line_color) {

	// if the slope is too steep, we should flip the coordinates, since this draws a smoother line
	bool is_steep = abs(y1 - y0) > abs(x1 - x0);
	// flip the coordinates. later we will draw a flipped line to compensate
	if (is_steep) {
		swap(&x0, &y0);
		swap(&x1, &y1);
	}

	// if the starting coordinate is greater, swap coords since drawing the line
	// backwards is the same as drawing it forwards
	if (x0 > x1) {
		swap(&x0, &x1);
		swap(&y0, &y1);
	}

	int deltaX = x1 - x0;
	int deltaY = abs(y1 - y0);
	int accumulated_error = -(deltaX / 2);
	int y = y0;

	int y_inc;
	// set the y_increment, 1 if increasing downwards (+ve slope), -1 if increasing upwards (-ve slope)
	if (y0 < y1) {
		y_inc = 1;
	} else {
		y_inc = -1;
	}

	// incrementing x to draw the line
	int x;
	for (x = x0; x <= x1; x++) {
		// draw the line flipped since we flipped the coordinates previously
		if (is_steep) {
			plot_pixel(y, x, line_color);
		} else {
			plot_pixel(x, y, line_color);
		}
		// accumulate the error over iterations
		accumulated_error += deltaY;
		// if the error overflows, increment y so that it follows the line
		if (accumulated_error >= 0) {
			y = y + y_inc;
			accumulated_error -= deltaX;
		}
	}
}

//code to condunct a buffer swap
void wait_for_vsync(){
    register int status;
	//writes a 1 to buffer register to start sync process
    *pixelControlPtr = 1;
	
	//continously reads the status register till it's 0 and then continues
	//this process allows the DMA to read the pixels from memory and output to VGA
	//when the status bit hits 0, it means the readings have been completed
    status = *(pixelControlPtr + 3);
    while((status & 0x01) != 0){
        status = *(pixelControlPtr + 3);
    }
}

void plot_pixel(int x, int y, short int line_color)
{
    *(short int *)(pixelBufferStart + (y << 10) + (x << 1)) = line_color;
}

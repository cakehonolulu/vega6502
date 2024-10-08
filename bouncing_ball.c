#include "primitive.h"
#include "framebuffer.h"

#include "rasterrage.h"

static semaphore_t video_initted;
static semaphore_t drawing_semaphore;

// Timing variables
absolute_time_t last_time;
float frame_time = 0.0f;
float fps = 0.0f;

#define GRID_COLOR 0xF81F // Pink color (R=255, G=0, B=255)
#define GRID_SPACING 10    // Distance between lines in the grid
#define GRID_ROWS 10       // Number of rows in the grid
#define GRID_COLUMNS 12    // Number of columns in the grid
#define PERSPECTIVE_FACTOR 0.05f  // How much the grid "zooms" as it gets closer


// Ball properties
float ball_x = SCREEN_WIDTH / 2.0f;
float ball_y = SCREEN_HEIGHT / 2.0f;
float ball_vx = 1.0f;  // Horizontal velocity
float ball_vy = 0.0f;  // Initial vertical velocity
float gravity = 0.2f;  // Constant downward force
float damping = 0.95f;  // Damping factor to simulate energy loss on bounce

float rotation = 0.0f;


void draw_perspective_grid(float perspective_factor) {
    int start_x = 20;  // Start 20 pixels from the left edge of the screen
    int end_x = SCREEN_WIDTH - 20;  // End 20 pixels from the right edge of the screen
    int grid_width = end_x - start_x;  // Width of the grid

    // Calculate spacing based on the grid width
    int grid_spacing = grid_width / GRID_COLUMNS;

    // Draw horizontal lines
    for (int i = 0; i <= GRID_ROWS; i++) {
        // Calculate the y position
        int y = i * GRID_SPACING;
        
        // Calculate perspective scale based on how close the row is to the bottom
        float scale = 1.0f + perspective_factor * ((float)SCREEN_HEIGHT - y) / SCREEN_HEIGHT;

        // Draw horizontal line from start_x to end_x with perspective scaling
        draw_line(start_x, y, end_x, y, GRID_COLOR);
    }

    int x, y_end;

    // Draw vertical lines
    for (int j = 0; j <= GRID_COLUMNS; j++) {
        // Calculate the x position
        x = start_x + j * GRID_SPACING;

        // Calculate perspective scaling based on the row number
        float scale = 1.0f + perspective_factor * ((float)SCREEN_HEIGHT - (GRID_ROWS * GRID_SPACING)) / SCREEN_HEIGHT;

        // Calculate the end point of the vertical line based on perspective scaling
        y_end = SCREEN_HEIGHT - 20;  // Prevent extending beyond SCREEN_HEIGHT - 30

        // Calculate the y position of the vertical line to not exceed the limit
        int y_start = 0; // Start from the top of the screen

        // Draw vertical line from y_start to y_end
        draw_line(x, y_start, x, y_end, GRID_COLOR);
    }

    draw_line(x, y_end, x + 20, SCREEN_HEIGHT, GRID_COLOR);
    draw_line(20, y_end, 0, SCREEN_HEIGHT, GRID_COLOR);

    draw_line(15, y_end + 5, x + 5, y_end + 5, GRID_COLOR);
    draw_line(9, y_end + 12, x + 12, y_end + 12, GRID_COLOR);
 
 // Draw 11 additional vertical lines starting from the last x position
    int last_x = x;
    int y_start = 0;  // Start from the top of the screen
    for (int i = 1; i < 6; i++) {
        int x_offset = last_x - i * GRID_SPACING;
        draw_line(x_offset, y_end, x_offset + 15 - i * 2, SCREEN_HEIGHT, GRID_COLOR);

        if (i == 5) last_x = x_offset - 10;
    }

    draw_line(last_x, y_end, last_x, SCREEN_HEIGHT, GRID_COLOR);

    //last_x -= 10;

    
    for (int i = 1; i < 6; i++) {
        int x_offset = last_x - i * GRID_SPACING;
        draw_line(x_offset, y_end, x_offset - i * 2 - 3, SCREEN_HEIGHT, GRID_COLOR);
    }

}

int main(void) {
    //set_sys_clock_khz(200000, true);
    stdio_init_all();
    // Initialize the pixel framebuffer with black pixels
    memset(pixel_framebuffer, 0, sizeof(pixel_framebuffer));

    // initialize video and interrupts on core 0
    scanvideo_setup(&vga_mode);
    scanvideo_timing_enable(true);
    sem_release(&video_initted);

    // create a semaphore to be posted when video init is complete
    sem_init(&video_initted, 0, 1);

    // launch all the video on core 1
    multicore_launch_core1(core1_func);

    set_clear_colour(PICO_SCANVIDEO_PIXEL_FROM_RGB5(148, 148, 148));
        
    while (true) {
        absolute_time_t start_time = get_absolute_time();
        
        // Update ball position based on velocity
        ball_x += ball_vx;
        ball_y += ball_vy;

        // Apply gravity to the vertical velocity
        ball_vy += gravity;

        // Check for collisions with screen edges and bounce
        if (ball_x - 25 < 0 || ball_x + 25 > SCREEN_WIDTH) {
            ball_vx *= -1; // Invert horizontal velocity
        }
        if (ball_y + 25 > SCREEN_HEIGHT) {
            ball_y = SCREEN_HEIGHT - 25; // Ensure the ball doesn't go below the screen
            ball_vy = -ball_vy * damping; // Reverse and reduce vertical velocity to simulate bounce

            // Stop bouncing when the velocity is too small
            if (fabs(ball_vy) < 1.0f) {
                ball_vy = 0.0f;
            }
        }

        draw_perspective_grid(PERSPECTIVE_FACTOR);

        int shadow_offset_x = 5;  // Horizontal offset of the shadow
        int shadow_offset_y = 5;  // Vertical offset of the shadow
        int radius = 20;  // Shadow size (same as the sphere)
        uint16_t shadow_color = 0x8410; // Gray color for shadow (R=128, G=128, B=128)

        // Draw the shadow as a 2D filled circle
        draw_filled_circle((int)ball_x + shadow_offset_x, (int)ball_y + shadow_offset_y, radius, shadow_color);

        // Draw the sphere with the updated position
        float tilt_angle = 10.5f * (PI / 180.0f); 
        float right_tilt_angle = -75.5f * (PI / 180.0f);  // Tilt around Y-axis (tilt to the right)
        draw_sphere(8, 16, radius, (int)ball_x, (int)ball_y, rotation, tilt_angle, right_tilt_angle);

        absolute_time_t now = get_absolute_time();
        frame_time = (float)absolute_time_diff_us(start_time, now) / 1000.0f; // Convert to milliseconds
        fps = 1000.0f / frame_time;

        char info[40];
        
        snprintf(info, sizeof(info), "%.2f FPS", fps);
        cur_x = 0; cur_y = 0;
        write_smallstring_to_framebuffer(info, 0x0F);
        snprintf(info, sizeof(info), "%d tri", drawn_triangles);
        cur_x = 0; cur_y = 2;
        write_smallstring_to_framebuffer(info, 0x0F);
        snprintf(info, sizeof(info), "@cakehonolulu");
        cur_x = 7; cur_y = 0;
        write_string_to_framebuffer(info, 0x0F);


        // Increment rotation angle
        rotation += 0.05f; 

        if (rotation > 2 * PI) {
            rotation -= 2 * PI;
        }

        scanvideo_wait_for_vblank();   // Wait for vertical blanking
        swap_framebuffers(); 
    }
}

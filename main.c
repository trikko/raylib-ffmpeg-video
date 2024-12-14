#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <raylib.h>
#include <ctype.h>

int main() {

    // It can be a local file or a remote file. In this case it's a video from wikipedia
    const char *file = "https://upload.wikimedia.org/wikipedia/commons/5/5f/Steamboat_Willie_%281928%29_by_Walt_Disney.webm";

    // FFMPEG command to generate raw RGBA output
    // Audio is sent to the default audio device using PulseAudio
    // To use ALSA, try -f alsa default instead of -f pulse default
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "ffmpeg -re -i %s -f pulse default -f rawvideo -pix_fmt rgba - 2>/dev/null", file);

    // FFMPEG command to get information on stderr
    char info_cmd[1024];
    snprintf(info_cmd, sizeof(info_cmd), "ffmpeg -i %s 2>&1", file);

    // Open ffmpeg to read information
    FILE *info_pipe = popen(info_cmd, "r");
    if (!info_pipe) {
        perror("Error opening ffmpeg for information. Did you install ffmpeg?");
        return 1;
    }

    // Variables to determine resolution
    int frame_width = 0, frame_height = 0;
    char line[1024];

    // Read information from stderr
    while (fgets(line, sizeof(line), info_pipe)) {

        // Search for video dimensions (example: 640x480)
        if (strstr(line, "Stream #") && strstr(line, "Video:")) {
            char *res = strstr(line, "x");
            if (res) {
                char *width_start = res - 1;
                while (width_start > line && isdigit(*(width_start - 1))) {
                    width_start--;
                }

                sscanf(width_start, "%dx%d", &frame_width, &frame_height);
                break;
            }
        }
    }

    pclose(info_pipe);

    // Open ffmpeg to read frames
    FILE *pipe = popen(cmd, "r");
    if (!pipe) {
        perror("Error opening ffmpeg for frames. Did you install ffmpeg?");
        return 1;
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(800, 450, "FFMPEG + Raylib");
    SetTargetFPS(60);

    unsigned int frame_size = frame_width * frame_height * 4; // RGBA: 4 bytes per pixel

    // Buffer to hold a frame
    uint8_t *frame = MemAlloc(frame_size);

    // Create the texture
    Image image = {
        .data = frame,
        .width = frame_width,
        .height = frame_height,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
        .mipmaps = 1
    };

    Texture2D texture = LoadTextureFromImage(image);

    // I want to scale the image to fit the window
    float scaleX = 800.0f / frame_width;
    float scaleY = 450.0f / frame_height;
    float scale = scaleX < scaleY ? scaleX : scaleY;
    // Add a variable for the mouse zoom factor
    float mouse_zoom = 0.75f;

    // Calculate the final dimensions
    int dest_width = frame_width * scale;
    int dest_height = frame_height * scale;

    while (!WindowShouldClose()) {
        // Update the zoom factor with the mouse wheel
        float wheel = GetMouseWheelMove();
        if (wheel != 0) {
            mouse_zoom += wheel * 0.1f;
            // Limitiamo il zoom tra 0.5 (1/2) e 1.0
            if (mouse_zoom < 0.5f) mouse_zoom = 0.5f;
            if (mouse_zoom > 1.0f) mouse_zoom = 1.0f;
        }

        // Calculate the final dimensions with mouse zoom
        int dest_width = frame_width * scale * mouse_zoom;
        int dest_height = frame_height * scale * mouse_zoom;

        // Read a frame from ffmpeg output
        size_t read_bytes = fread(frame, 1, frame_size, pipe);

        if (read_bytes < frame_size) {
            if (feof(pipe)) {
                break;
            } else {
                perror("Error while reading a frame");
                break;
            }
        }

        // Update the texture with the new frame
        UpdateTexture(texture, frame);

        // Get mouse position
        int mouse_x = GetMouseX() - dest_width/2;
        int mouse_y = GetMouseY() - dest_height/2;

        // Draw the texture
        BeginDrawing();

        ClearBackground((Color){147, 155, 159, 255});

        DrawRectangle(mouse_x-5, mouse_y-5, dest_width+10, dest_height+10, (Color){60, 60, 60, 255});

        // Draw the texture
        DrawTexturePro(texture,
            (Rectangle){0, 0, frame_width, frame_height},
            (Rectangle){mouse_x, mouse_y, dest_width, dest_height},
            (Vector2){0, 0}, 0.0f, WHITE);

        EndDrawing();
    }

    // Cleanup
    UnloadTexture(texture);
    free(frame);
    pclose(pipe);

    return 0;
}
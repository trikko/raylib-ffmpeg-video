/+ dub.sdl:
name "raylib-ffmpeg-video"
description "FFMPEG + Raylib video player"
authors "Andrea Fontana"
copyright "Copyright © 2024, Andrea Fontana"
license "MIT"
dependency "raylib-d" version="~>5.5.1"
libs "raylib"
lflags "-L."
lflags-posix "-rpath=$$ORIGIN"
lflags-osx "-rpath=@executable_path/"
lflags-linux "-rpath=$$ORIGIN"
+/

import std;
import raylib;

void main()
{
	validateRaylibBinding();

	// It can be a local file or a remote file. In this case it's a video from wikipedia
	string file = "https://upload.wikimedia.org/wikipedia/commons/5/5f/Steamboat_Willie_%281928%29_by_Walt_Disney.webm";

	// FFMPEG command to generate raw RGBA output
	// Audio is sent to the default audio device using PulseAudio
	string cmd = format("ffmpeg -re -i %s -f pulse default -f rawvideo -pix_fmt rgba - 2>/dev/null", file);

	// FFMPEG command to get information on stderr
	string info_cmd = format("ffmpeg -i %s 2>&1", file);
	int frame_width = 0, frame_height = 0;

	// Open ffmpeg to read information
	auto infos = pipeShell(info_cmd, Redirect.all);

	while(!infos.stdout.eof())
	{
		auto line = infos.stdout.readln();
		auto matches = line.matchAll(regex(r"Stream #.*Video:.*, (\d+x\d+),"));

		if (matches)
		{
			auto dimensions = matches.captures[1].split('x');
			frame_width = dimensions[0].to!int;
			frame_height = dimensions[1].to!int;
			break;
		}
	}

	// Kill ffmpeg
	infos.pid.kill();

	// Open ffmpeg to read frames
	auto pipe = pipeShell(cmd, Redirect.all);

	SetConfigFlags(ConfigFlags.FLAG_WINDOW_RESIZABLE | ConfigFlags.FLAG_VSYNC_HINT);
	InitWindow(800, 450, "FFMPEG + Raylib");
	SetTargetFPS(60);

	size_t frame_size = frame_width * frame_height * 4; // RGBA: 4 bytes per pixel

	// Buffer to hold a frame
	ubyte[] frame;
	frame.length = frame_size;

	// Create the texture
	Image image = Image(
		data: frame.ptr,
		width: frame_width,
		height: frame_height,
		format: PixelFormat.PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
		mipmaps: 1
	);

	Texture2D texture = LoadTextureFromImage(image);

	// Scale the image to fit the window
	float scale = min(800.0f / frame_width, 450.0f / frame_height);
	float mouse_zoom = 0.75f;

	// Calculate the final dimensions
	int dest_width = cast(int)(frame_width * scale);
	int dest_height = cast(int)(frame_height * scale);

	while (!WindowShouldClose()) {
		// Update the zoom factor with the mouse wheel
		float wheel = GetMouseWheelMove();
		if (wheel != 0) {
			mouse_zoom += wheel * 0.1f;
			mouse_zoom = clamp(mouse_zoom, 0.5f, 1.0f);
		}

		// Calculate the final dimensions with mouse zoom
		dest_width = cast(int)(frame_width * scale * mouse_zoom);
		dest_height = cast(int)(frame_height * scale * mouse_zoom);

		// Read a frame from ffmpeg output
		auto bytes = pipe.stdout.rawRead(frame);

		if (bytes.length < frame_size) {
			if (pipe.stdout.eof()) {
				break;
			} else {
				writeln("Error while reading a frame");
				break;
			}
		}

		// Update the texture with the new frame
		UpdateTexture(texture, frame.ptr);

		// Get mouse position
		int mouse_x = GetMouseX() - dest_width/2;
		int mouse_y = GetMouseY() - dest_height/2;

		// Draw the texture
		BeginDrawing();

		ClearBackground(Color(147, 155, 159, 255));

		DrawRectangle(mouse_x-5, mouse_y-5, dest_width+10, dest_height+10, Color(60, 60, 60, 255));

		DrawTexturePro(texture,
			Rectangle(0, 0, frame_width, frame_height),
			Rectangle(mouse_x, mouse_y, dest_width, dest_height),
			Vector2(0, 0), 0.0f, Colors.WHITE);

		EndDrawing();
	}

	// Cleanup
	UnloadTexture(texture);
}

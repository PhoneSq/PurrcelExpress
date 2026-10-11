🐾 Purrcel Express

A cat delivery roguelike. You're a cat on foot with a package and a deadline. Your job is to get it across the neighborhood without the dogs catching you.

Purrcel Express is written from scratch in C++ on top of the raw Win32 API, with no game engine and no SDL. It draws everything with a hand-written CPU raycaster at 640×400, doubled to fit the window.

⚠️ Windows only. The game uses the Win32 API, GDI, GDI+ and winmm (waveOut audio) directly, so it won't build or run on macOS or Linux. Windows 10 or 11 is recommended.

🎮 How it plays
Pick your cat. Each cat has its own ability.
Pick a delivery. Contracts come in different difficulties and times of day, and some are timed Express jobs that pay a bonus.
Visit the shop if you want an item for the run. You can carry one at a time.
Deliver the package to the doorstep, then head back to your bike at the starting point.
Avoid the dogs. Once one spots you (watch for the red !), it won't stop chasing you.
The cats
Cat	Ability	What it does
Fat Cat	Ground Pound	Jumps and slams down, knocking nearby dogs back
Furry Cat	Fur Barrage	Shoots fur balls for a few seconds. Any dog hit is stunned
Blind Cat	Dash	Floats forward about 2 tiles and can't be hurt while dashing
The dogs
Dog	Behavior
Chasey (blue)	Runs straight at you
Spiky (yellow)	Hops around and tries to get in your way
Blocky (purple)	Cuts you off

Dogs roam near their home spot. When one spots you, it chases. If you lose it, it goes back home.

Shop items
Item	Price	Effect
Health Potion	$150	+1 HP
Stamina Potion	$100	Refills your stamina
Bone	$120	Lures nearby dogs away for a few seconds
⌨️ Controls
Key	Action
W A S D	Move and strafe
Mouse / ← →	Turn
Shift	Sprint (uses stamina)
E	Use your cat's ability
Q	Use your held item
Left Click / Space	Fire (during Fur Barrage)
V	Switch between third-person and first-person view
Esc	Back

In the menus, use the arrows or W/S (or A/D) with Enter/Space, or the mouse. Press B on the delivery screen to open the shop.

🛠️ Built with
C++17
Win32 API for the window, input and game loop
GDI to blit the framebuffer and draw text
GDI+, used only to load PNG files
winmm (waveOut) for audio. It's a custom 16-voice software mixer on its own thread
Visual Studio as the IDE and compiler
MinGW-w64 for cross-compile checks
Baloo 2 font (SIL Open Font License)

Not used: game engines, SDL/SFML, OpenGL/DirectX, or the C++ STL. Containers, math and rendering are all hand-rolled.

Under the hood
DDA raycaster with a depth buffer, plus low walls (hedges and fences) you can see over
Sloped house roofs built from a heightmap
360° layered sky panorama with drifting clouds, and four times of day (morning, noon, sunset, night) that each tint the fog
Billboard sprites with a procedural walk cycle (bounce, squash and lean)
Procedurally generated neighborhood maps
Every sound effect is synthesized in code. There are no audio files
🚀 Building and running

Requirements: Windows and Visual Studio 2019 or newer, with the Desktop development with C++ workload.

Clone the repo.
Open the project in Visual Studio. If you're creating a new project, make it an empty Windows Desktop project and add every .h and .cpp file.
Build in Debug or Release (x64 recommended).
Make sure the assets\ folder sits next to the executable, or in the working directory when you run from Visual Studio.

You don't need to set up any libraries. user32, gdi32, gdiplus and winmm are all linked through #pragma comment and ship with Windows.

MinGW alternative:

x86_64-w64-mingw32-g++ -O2 -std=c++17 -mwindows *.cpp -o PurrcelExpress.exe -lgdi32 -luser32 -lgdiplus -lwinmm -static
📁 Project layout
File	What's in it
main.cpp	Entry point
Game	Window, input, game loop and state switching
Screen	Every screen: menu, character select, delivery select, shop, the run and game over
Renderer	Raycaster, sprites, sky, roofs and textures
World	Map generation, tiles and delivery contracts
Player	Movement, stamina, HP and abilities
Enemy	Dog AI (roam, chase, lured, return home)
Character	The playable cats and their abilities
Item	Shop items and their effects
Audio	waveOut mixer
AItexture	Procedurally generated art (sky, foliage, roofs, icons)
AISound	Procedurally synthesized sound effects
Config.h	All the tuning numbers in one place
assets/	PNG sprites, menu art and fonts
🤖 AI usage

The game was built with help from an AI assistant (Claude). The AI-generated art and sound live in their own files, so they're easy to tell apart:

AItexture.h/.cpp holds the procedurally drawn textures and icons.
AISound.h/.cpp holds the synthesized sound effects.
The main menu art was not AI-made.

The character, dog and item sprites in assets/ are hand-drawn.

📜 Credits
Game by Phone Myat Min and Phyo Hein Htet
Font: Baloo 2 by Ek Type (SIL OFL 1.1)

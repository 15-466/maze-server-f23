# Nest

This and other 15-466 base code is built around a collection of useful libraries and functions which we will collectively call "nest".
This name captures the goal of having the various parts and functions relatively easy to reconfigure (and remove), while still forming a good support for your game.

## Build Instructions

This code is set up to build across Linux, Windows, and MacOS.

Building will be done from the command prompt, using Maek.
Maek is a build system implemented as a single javascript file.
To add or remove files from the build, read and edit [Maekfile.js](Maekfile.js).

### Setup

Setup for your development environment should be relatively simple:

 0. (Optional) Make sure your system is set up to make it easy to use your favorite code editor and git from the command prompt. So much nicer than using a GUI.
 1. Install one of our target C++ compilers:
 	- Linux: g++
	- MacOS: clang++ (from XCode). From the terminal: `xcode-select --install`
	- Windows: Visual Studio Community 2022
 2. Install [node](https://node.js):
    - Linux: e.g. `sudo apt-get install node`
	- MacOS: e.g. `brew install node`
	- Windows: [download from node's web page](https://nodejs.org/en/download/)
 3. Extract an appropriate [release of nest-libs](https://github.com/15-466/nest-libs/releases) to a sibling of this folder:
	- Linux: https://github.com/15-466/nest-libs/releases/download/v0.13/nest-libs-linux-v0.13.tar.gz
	- MacOS: https://github.com/15-466/nest-libs/releases/download/v0.13/nest-libs-macos-v0.13.tar.gz
	- Windows: https://github.com/15-466/nest-libs/releases/download/v0.13/nest-libs-windows-v0.13.zip

Once you are finished, your directory tree should looks something like this:

```
game-programming/ #can be called anything
	nest-libs/    #nest-libs from that repository's releases page
		windows/  #subdirectory name varies depending on platform
		...
	my-gameN/     #fork of the baseN code; can be called anything
		...
	...           #further subdirectories for other games
```

### Building

Once you have your development environment set up, building is as simple as opening a command prompt (see Windows Note below), changing to the game directory, and running `node Maekfile.js`.

Here are a few worthwhile variations:

```
# Simplest build command, runs as many tasks as you have CPUs:
  $ node Maekfile.js

# Variation: limit parallelism to one task at once:
  $ node Maekfile.js -j1

# Variation: run one task at once and quit if any job fails:
  $ node Maekfile.js -q -j1

# Variation: run game if build succeeds:
  $ node Maekfile.js && dist/pong

# Variation: show commands being run (perhaps useful for debugging):
  $ node Maekfile.js -v
```

*Windows Note:* you will need to use a command prompt with the visual studio tools and variables configured. The "x64 Native Tools Command Prompt for VS2022" start menu option provides this option.

## A Word About Github Actions

This repository is equipped with a `.github/workflows/build-workflow.yml` file that tells github that you would like it to build the code for you whenever you push code.
This is a great way to check if things are working cross-platform and even to package releases of your game (the workflow is set up such that if you create a release through the github web UI, it will automatically build, package, and upload binaries to the release).

It can also be a frustrating and time-wasting trap to try to debug any build failures solely using github actions. Use it as a check, but not a development environment.

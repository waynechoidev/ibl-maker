## Dependencies

Before building the project, make sure you have the following dependencies installed:

```bash
sudo apt install g++ build-essential cmake libopengl0 libopengl-dev libglfw3 libglfw3-dev
```

## Submodules

This project uses Git submodules to manage external dependencies. After cloning the repository, make sure to initialize and update the submodules using the following commands:

```bash
git submodule init
git submodule update
```

## Credits

This project was developed with reference to the content available at [Learn OpenGL](https://learnopengl.com/). The ideas presented on that website served as inspiration for this implementation. Additionally, the code from [IBL Baker](https://www.derkreature.com/iblbaker) was also consulted for further enhancements.

## License

This project is licensed under the  [CC BY-NC 4.0](https://creativecommons.org/licenses/by-nc/4.0/).
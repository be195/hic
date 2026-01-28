# hic
hic is a stupidly simple helper/game engine 
inspired by web frontend frameworks like
React or Vue.

## core concept
it introduces BaseComponent, which:
- can be inherited from to make new components
- can have its own child components and,
  therefore, parent
- eases the game and general UI (for the game)
  development

```c++
class MyGame : public hic::BaseComponent {
  std::shared_ptr<Player> player;
  std::shared_ptr<UI> ui;
  
  void preload() override {
    // request to load assets (asset manager runs in a worker thread)
    font = container->getAssetManager()->load<Assets::BitmapFont>("arial");
  }
  
  void mounted() override {
    // create child components
    player = std::make_shared<Player>();
    ui = std::make_shared<UI>();
    addChild(player);
    addChild(ui);
  }
  
  void update(float deltaTime, float time) override {
    // game logic
  }
  
  void render(SDL_Renderer* r, float time) override {
    // rendering
  }
};
```

BaseComponent and hic also implements multiple
features, like:
- scene management
- threaded asset loading
- audio system built on top of libopus and oggfile
- built-in animation mixin
- input event handling
- viewport/clipping
- optimization goodies (like fps limiting)

## component lifecycle
```c++
class MyComponent : public hic::BaseComponent {
  void preload() override {
    // called once on first mount - load assets here
    // don't get confused! - preload runs on main thread
  }

  void mounted() override {
    // called after mounting - initialize state
  }

  void update(float deltaTime, float time) override {
    // called every frame
  }

  void render(SDL_Renderer* r, float time) override {
    // render this component
  }

  void destroy() override {
    // cleanup before destruction (not to be
    // confused with actual instance destruction)
  }
};
```

## building

### prerequisites
- CMake 3.15+
- C++20 compiler

that's about it. supplied CPM should download
third-party libs (e.g. SDL).

### CMake options
by default, it will be built for static
use, however you can build it for shared
use using `HIC_BUILD_SHARED` option.

### redistribution
SDL3 is built as a shared library. you must
redistribute the built file with your
executable.

## what does hic stand for?
hic doesn't stand for anything, really. pick
your poison:
- hiccup *(hic!)*
- hierarchical interactive components
- hell is coming
- hey, i cheat
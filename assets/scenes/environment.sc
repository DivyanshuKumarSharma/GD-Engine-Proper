{
  "name": "Training Range",
  "camera": "MainPlayer",
  "objects": [
    {
      "name": "3DRoot",
      "children": [
        {
          "name": "MainPlayer",
          "type": "Player",
          "config": "config/player.json",
          "position": { "x": 0, "y": 2, "z": 14 },
          "components": [
            { "type": "CameraComponent" },
            {
              "type": "PlayerControllerComponent",
              "moveSpeed": 50.0,
              "lookSensitivity": 15.0,
              "jumpSpeed": 5.0,
              "collider": { "radius": 0.4, "height": 1.2 },
              "actions": {
                "forward": "MoveForward", "backward": "MoveBackward",
                "left": "MoveLeft", "right": "MoveRight", "jump": "Jump"
              }
            },
            { "type": "AudioListenerComponent" },
            {
              "type": "AudioComponent",
              "audio": [
                { "name": "shoot", "path": "audio/shoot.wav" },
                { "name": "step", "path": "audio/step.wav" },
                { "name": "jump", "path": "audio/jump.wav" }
              ]
            }
          ]
        },
        {
          "name": "Ground",
          "position": { "x": 0, "y": 0, "z": 0 },
          "components": [
            { "type": "MeshComponent",
              "mesh": { "type": "box", "x": 40, "y": 1, "z": 40 },
              "material": { "path": "materials/checker.mat",
                "params": { "float3": [ { "name": "color", "value0": 0.62, "value1": 0.64, "value2": 0.68 } ] } } },
            { "type": "PhysicsComponent",
              "collider": { "type": "box", "x": 40, "y": 1, "z": 40 },
              "body": { "mass": 0, "friction": 0.8, "type": "static" } }
          ]
        },
        {
          "name": "WallNorth",
          "position": { "x": 0, "y": 3.0, "z": -20.5 },
          "components": [
            { "type": "MeshComponent", "mesh": { "type": "box", "x": 41, "y": 6, "z": 1 },
              "material": { "path": "materials/checker.mat",
                "params": { "float3": [ { "name": "color", "value0": 0.5, "value1": 0.53, "value2": 0.58 } ] } } },
            { "type": "PhysicsComponent", "collider": { "type": "box", "x": 41, "y": 6, "z": 1 },
              "body": { "mass": 0, "friction": 0.5, "type": "static" } }
          ]
        },
        {
          "name": "WallSouth",
          "position": { "x": 0, "y": 3.0, "z": 20.5 },
          "components": [
            { "type": "MeshComponent", "mesh": { "type": "box", "x": 41, "y": 6, "z": 1 },
              "material": { "path": "materials/checker.mat",
                "params": { "float3": [ { "name": "color", "value0": 0.5, "value1": 0.53, "value2": 0.58 } ] } } },
            { "type": "PhysicsComponent", "collider": { "type": "box", "x": 41, "y": 6, "z": 1 },
              "body": { "mass": 0, "friction": 0.5, "type": "static" } }
          ]
        },
        {
          "name": "WallWest",
          "position": { "x": -20.5, "y": 3.0, "z": 0 },
          "components": [
            { "type": "MeshComponent", "mesh": { "type": "box", "x": 1, "y": 6, "z": 40 },
              "material": { "path": "materials/checker.mat",
                "params": { "float3": [ { "name": "color", "value0": 0.5, "value1": 0.53, "value2": 0.58 } ] } } },
            { "type": "PhysicsComponent", "collider": { "type": "box", "x": 1, "y": 6, "z": 40 },
              "body": { "mass": 0, "friction": 0.5, "type": "static" } }
          ]
        },
        {
          "name": "WallEast",
          "position": { "x": 20.5, "y": 3.0, "z": 0 },
          "components": [
            { "type": "MeshComponent", "mesh": { "type": "box", "x": 1, "y": 6, "z": 40 },
              "material": { "path": "materials/checker.mat",
                "params": { "float3": [ { "name": "color", "value0": 0.5, "value1": 0.53, "value2": 0.58 } ] } } },
            { "type": "PhysicsComponent", "collider": { "type": "box", "x": 1, "y": 6, "z": 40 },
              "body": { "mass": 0, "friction": 0.5, "type": "static" } }
          ]
        },
        {
          "name": "KeyLight",
          "position": { "x": 8, "y": 18, "z": 12 },
          "components": [
            { "type": "LightComponent", "color": { "r": 1.0, "g": 0.98, "b": 0.94 } }
          ]
        }
      ]
    }
  ]
}

{
  "version": "1.3.12",
  "vars": {
    "PAG_GROUP": "https://github.com/libpag"
  },
  "repos": {
    "common": [
      {
        "url": "${PAG_GROUP}/vendor_tools.git",
        "commit": "b0af769cf0d29593e4d9d6d5872a87cfa8f9fa93",
        "dir": "third_party/vendor_tools"
      },
      {
        "url": "${PAG_GROUP}/tgfx.git",
        "commit": "b91b8dd7cb54f9956feea7b0e9a0b3afb260960e",
        "dir": "third_party/tgfx"
      },
      {
        "url": "https://skia.googlesource.com/external/github.com/emscripten-core/emsdk.git",
        "commit": "9dbdc4b3437750b85d16931c7c801bb71a782122",
        "dir": "third_party/emsdk"
      }
    ]
  },
  "actions": {
    "common": [
      {
        "command": "depsync --clean",
        "dir": "third_party"
      },
      {
        "command": "node ../../install_emsdk.js",
        "dir": "third_party/emsdk"
      }
    ]
  }
}
# Wasm Renderer Notes

## Initial Audit

Current renderer signs are favorable for WebGL:

- No fixed-function `glBegin/glEnd` paths found.
- Shaders use GLSL ES 2 style syntax.
- The code already checks `GLAD_GL_ES_VERSION_2_0` in several places.
- Cubemap setup already avoids `GL_TEXTURE_WRAP_R` on ES2.

## Areas To Verify In Browser Runtime

- framebuffer/post-process compatibility
- cubemap and skybox rendering in WebGL
- depth and stencil behavior
- multisampling expectations
- any extension-dependent paths hidden behind SeriousProton abstractions

## Known Constraints

- The browser build should avoid assuming desktop core-profile behavior.
- WebGL 2 is preferred when available, but the code should stay close to GLES2-compatible behavior where practical.
- Any unsupported GL usage discovered during runtime testing should be logged here as follow-up items.

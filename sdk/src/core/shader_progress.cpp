#include <rex/graphics/shader_progress.h>

namespace rex::graphics {

ShaderProgress& GetShaderProgress() {
  static ShaderProgress progress;
  return progress;
}

}  // namespace rex::graphics

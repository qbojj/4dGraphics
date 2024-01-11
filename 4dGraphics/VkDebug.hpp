#include <Device.hpp>

#include <glm.hpp>

namespace v4dg {
class CmdDebugLabel {
public:
  CmdDebugLabel(vk::raii::CommandBuffer &cmd, const char *name,
                const glm::vec4 &color = {1, 1, 1, 1})
#ifdef V4DG_PRODUCTION
      {}
#else
      : m_cmd(cmd) {

    m_cmd.beginDebugUtilsLabelEXT(vk::DebugUtilsLabelEXT{
        .pLabelName = name,
        .color = color,
    });
  }
#endif

#ifndef V4DG_PRODUCTION
  ~CmdDebugLabel() {
    m_cmd.endDebugUtilsLabelEXT();
  }

private:
  vk::raii::CommandBuffer &m_cmd;
#endif
};

} // namespace v4dg
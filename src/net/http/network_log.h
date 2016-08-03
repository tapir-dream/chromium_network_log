#ifndef NET_HTTP_NETWORK_LOG_H_
#define NET_HTTP_NETWORK_LOG_H_

#include <string>
#include <vector>

namespace net {

class NET_EXPORT Network_log  {

public:

  Network_log();
  ~Network_log();

 static Network_log* Instance() {
    if (!m_pInstance)
      m_pInstance = new Network_log;
    return m_pInstance;
  }

  void setValue(const std::string entry) {
    headers_.push_back(entry);
  }

 std::vector<std::string> headers() {
    return headers_;
  }

  void getHeaders(std::vector<std::string>* list) {
    uint32_t count = headers_.size();
    for (uint32_t i = 0; i < count; i++) {
      list->push_back(headers_.at(i));
     }
  }

private:
   static Network_log* m_pInstance;
   std::vector<std::string> headers_;

};

}

#endif  // NET_HTTP_NETWORK_LOG_H_
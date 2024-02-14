#pragma once

#include <systemd/sd-event.h>

#include <functional>
#include <string>
#include <filesystem>

namespace phosphor
{
namespace software
{
namespace manager
{

namespace fs = std::filesystem;

/** @class Watch
 *
 *  @brief Adds inotify watch on software image upload directory
 *
 *  The inotify watch is hooked up with sd-event, so that on call back,
 *  appropriate actions related to a software image upload can be taken.
 */
class Watch
{
  public:
    /** @brief ctor - hook inotify watch with sd-event
     *
     *  @param[in] loop - sd-event object
     *  @param[in] imageCallback - The callback function for processing
     *                             the image
     */
    Watch(sd_event* loop, std::function<int(std::string&)> imageCallback);

#ifdef NVIDIA_SECURE_BOOT
    /** @brief ctor - hook inotify watch with sd-event
     *
     *  @param[in] loop - sd-event object
     *  @param[in] path - filepath object
     *  @param[in] imageCallback - The callback function for processing
     *                             the image
     */
    Watch(sd_event* loop, const fs::path& path,
          std::function<int(std::string&)> imageCallback);
#endif

    Watch(const Watch&) = delete;
    Watch& operator=(const Watch&) = delete;
    Watch(Watch&&) = delete;
    Watch& operator=(Watch&&) = delete;

    /** @brief dtor - remove inotify watch and close fd's
     */
    ~Watch();

#ifdef NVIDIA_SECURE_BOOT
    /** @brief File path to be watched */
    fs::path path;
#endif

  private:
    /** @brief sd-event callback
     *
     *  @param[in] s - event source, floating (unused) in our case
     *  @param[in] fd - inotify fd
     *  @param[in] revents - events that matched for fd
     *  @param[in] userdata - pointer to Watch object
     *  @returns 0 on success, -1 on fail
     */
    static int callback(sd_event_source* s, int fd, uint32_t revents,
                        void* userdata);

#ifdef NVIDIA_SECURE_BOOT
    /** @brief adds watch on path given as an argument
     *
     *  @param[in] loop - sd-event object
     *  @param[in] path - filepath object
     */
    void createInotify(sd_event* loop, const fs::path& path);
#endif

    /** @brief image upload directory watch descriptor */
    int wd = -1;

    /** @brief inotify file descriptor */
    int fd = -1;

    /** @brief The callback function for processing the image. */
    std::function<int(std::string&)> imageCallback;
};

} // namespace manager
} // namespace software
} // namespace phosphor

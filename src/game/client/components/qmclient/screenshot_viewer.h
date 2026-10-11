#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_SCREENSHOT_VIEWER_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_SCREENSHOT_VIEWER_H

// 桌面平台使用当前程序的专用看图进程，不初始化游戏、网络或配置存储。
bool OpenQmScreenshotViewer(const char *pPath, const char *pErrorTitle, const char *pErrorMessage);
int RunQmScreenshotViewer(const char *pPath, const char *pErrorTitle, const char *pErrorMessage);

#endif

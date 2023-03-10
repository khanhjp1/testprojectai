#ifndef _ZIPARCHIVE_H_
#define _ZIPARCHIVE_H_

#include "cocos2d.h"
#include "cocos-ext.h"
using namespace std;
USING_NS_CC;
NS_CC_EXT_BEGIN
class CC_EX_DLL ZipArchive
{
public:
	ZipArchive();
	~ZipArchive();

	void startDecompression(const std::string &zip);

	bool decompress();
	int getCurProgress();
	int m_curProgress;
	string m_zipName;
	bool m_isStart;
private:
	mutex				m_mutex;
	std::string basename(const std::string& path) const;
	FileUtils *_fileUtils;
};
NS_CC_EXT_END
#endif

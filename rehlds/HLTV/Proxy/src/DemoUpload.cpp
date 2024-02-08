/*
*
*    This program is free software; you can redistribute it and/or modify it
*    under the terms of the GNU General Public License as published by the
*    Free Software Foundation; either version 2 of the License, or (at
*    your option) any later version.
*
*    This program is distributed in the hope that it will be useful, but
*    WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
*    General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with this program; if not, write to the Free Software Foundation,
*    Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*
*    In addition, as a special exception, the author gives permission to
*    link the code of this program with the Half-Life Game Engine ("HL
*    Engine") and Modified Game Libraries ("MODs") developed by Valve,
*    L.L.C ("Valve").  You must obey the GNU General Public License in all
*    respects for all of the code used other than the HL Engine and MODs
*    from Valve.  If you modify this file, you may extend this exception
*    to your version of the file, but you are not obligated to do so.  If
*    you do not wish to do so, delete this exception statement from your
*    version.
*
*/

#include "precompiled.h"

#include <curl/curl.h>
#include <pthread.h>

bool DemoUpload::Init(IBaseSystem *system, int serial, char *name)
{
	if (!BaseSystemModule::Init(system, serial, name))
		return false;

	auto curl_global_init_result_code = curl_global_init(CURL_GLOBAL_ALL);
	if (curl_global_init_result_code != CURLE_OK)
	{
		m_System->Printf("ERROR: Demo upload failed to initialize cURL: %d\n", curl_global_init_result_code);
		return false;
	}

	m_State = MODULE_RUNNING;
	m_System->Printf("Demo upload initialized.\n");

	return true;
}

void DemoUpload::ShutDown()
{
	if (m_State == MODULE_DISCONNECTED)
		return;

	curl_global_cleanup();

	BaseSystemModule::ShutDown();
	m_System->Printf("Demo upload shutdown.\n");
}

void DemoUpload::SetPushUrl(const char *pushUrl)
{
	Q_strncpy(m_PushUrl, pushUrl, sizeof(m_PushUrl));
}

void DemoUpload::Upload(const char *demoFilePath) const
{
	if (!m_PushUrl[0])
		return;

	m_System->Printf("Beginning background upload of demo %s\n", demoFilePath);
	auto uploadThreadArguments = new UploadThreadArguments;

	uploadThreadArguments->demoFilePath = Mem_Strdup(demoFilePath);
	uploadThreadArguments->pushUrl = Mem_Strdup(m_PushUrl);

	pthread_t demoUploadThread{};
	pthread_create(&demoUploadThread, nullptr, UploadThread, uploadThreadArguments);
}

void *DemoUpload::UploadThread(void *args)
{
	// FIXME: Better output (hard to do from a separate thread)

	auto uploadThreadArguments = static_cast<UploadThreadArguments*>(args);

	// This is mostly copied from Postman.
	CURL *curl{};
	CURLcode curl_result_code{};
	curl = curl_easy_init();

	if (curl)
	{
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
		curl_easy_setopt(curl, CURLOPT_URL, uploadThreadArguments->pushUrl);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_DEFAULT_PROTOCOL, "https");
		curl_slist *headers{};
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
		curl_mime *mime{};
		curl_mimepart *part{};
		mime = curl_mime_init(curl);
		part = curl_mime_addpart(mime);
		curl_mime_name(part, "files[0]");
		curl_mime_filedata(part, uploadThreadArguments->demoFilePath);
		part = curl_mime_addpart(mime);
		curl_mime_name(part, "payload_json");
		curl_mime_data(part, "{\"content\":\"A demo has been recorded.\"}", CURL_ZERO_TERMINATED);
		curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
		curl_result_code = curl_easy_perform(curl);

		curl_mime_free(mime);

		curl_easy_cleanup(curl);
	}

	Mem_Free(uploadThreadArguments->demoFilePath);
	Mem_Free(uploadThreadArguments->pushUrl);
	delete uploadThreadArguments;
	return nullptr;
}

/*
Copyright 2016-2017 Nia Catlin

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

/*
Header for the thread that reads trace information from drgat and buffers it
*/
#include "stdafx.h"
#include "thread_trace_reader.h"

vector<string *> * thread_trace_reader::get_read_queue()
{
	if (readingFirstQueue)
		return &secondQueue;
	else
		return &firstQueue;
}

void thread_trace_reader::add_message(string *newMsg)
{
	EnterCriticalSection(&flagCritsec);
	vector<string *> *targetQueue = get_read_queue();

	if (targetQueue->size() >= traceBufMax)
	{
		cout << "[rgat]Warning: Trace queue full with " << traceBufMax << " items! Waiting for processor to catch up..." << endl;
		LeaveCriticalSection(&flagCritsec);
		if (thisgraph)
			thisgraph->setBacklogIn(0);
		do {
			
			Sleep(500);

			EnterCriticalSection(&flagCritsec);

			targetQueue = get_read_queue();
			if (targetQueue->size() < traceBufMax/2) 
				break;
			if (targetQueue->size() <  traceBufMax/10)
				cout << "[rgat]Trace queue now "<< targetQueue->size() << "items" << endl;
			LeaveCriticalSection(&flagCritsec);

		} while (!die);
		cout << "[rgat]Trace queue now "<< targetQueue->size() << " items, resuming." << endl;
	}

	targetQueue->push_back(newMsg);
	pendingData += newMsg->size();

	if(!die)
		LeaveCriticalSection(&flagCritsec);
}

string *thread_trace_reader::get_message()
{
	
	if (readingQueue->empty() || readIndex >= readingQueue->size())
	{
		EnterCriticalSection(&flagCritsec);
		if (!readingQueue->empty())
		{
			vector<string *>::iterator queueIt = readingQueue->begin();
			for (; queueIt != readingQueue->end(); ++queueIt)
				delete *queueIt;
			readingQueue->clear();
		}
		readIndex = 0;

		if (readingFirstQueue)
		{
			readingQueue = &secondQueue;
			readingFirstQueue = false;
		}
		else
		{
			readingQueue = &firstQueue;
			readingFirstQueue = true;
		}

		if (processedData)
		{
			pendingData -= processedData;
			processedData = 0;
		}
		LeaveCriticalSection(&flagCritsec);
	}

	if (readingQueue->empty())
	{
		if (pipeClosed && firstQueue.empty() && secondQueue.empty()) return (string *)-1;
		return NULL;
	}

	string * nextMessage = readingQueue->at(readIndex++);
	processedData += nextMessage->size();
	return nextMessage;
}

bool thread_trace_reader::getBufsState(pair <size_t, size_t> *bufSizes)
{
	size_t q1Size = firstQueue.size();
	size_t q2Size = secondQueue.size();

	if (readingFirstQueue)
		q1Size -= readIndex;
	else
		q2Size -= readIndex;

	*bufSizes = make_pair(q1Size, q2Size);
	return readingFirstQueue; 
}

//thread handler to build graph for a thread
void thread_trace_reader::main_loop()
{
	alive = true;
	if (!threadpipe)
	{
		wstring pipename(L"\\\\.\\pipe\\rioThread");
		pipename.append(std::to_wstring(threadID));

		const wchar_t* szName = pipename.c_str();
		threadpipe = CreateNamedPipe(szName,
			PIPE_ACCESS_INBOUND, PIPE_TYPE_MESSAGE,
			1, //max instances
			1, //outbuffer
			1024 * 1024, //inbuffermax
			1, //timeout?
			NULL);

		if (threadpipe == (HANDLE)-1)
		{
			cerr << "[rgat]Error: Could not create pipe in thread handler " << threadID << ". error:" << GetLastError() << endl;
			alive = false;
			return;
		}

		ConnectNamedPipe(threadpipe, NULL);
	}

	vector <char> tagReadBuf;
	tagReadBuf.resize(TAGCACHESIZE);

	clock_t endwait = clock() + 1;
	unsigned long itemsRead = 0;

	DWORD bytesRead = 0;
	while (!die)
	{
		//should maybe have this as a timer but the QT one is more of a pain to set up
		clock_t secondsnow = clock();
		if (secondsnow > endwait)
		{
			endwait = secondsnow + 1;
			if(thisgraph)
				thisgraph->setBacklogIn(itemsRead);
			itemsRead = 0;
		}

		DWORD available;
		if(!PeekNamedPipe(threadpipe, NULL, NULL, NULL, &available, NULL) || !available)
		{
			int GLE = GetLastError();
			if (GLE == ERROR_BROKEN_PIPE) break;
			Sleep(5);
			continue;
		}

		if (!ReadFile(threadpipe, &tagReadBuf.at(0), (DWORD)tagReadBuf.size(), &bytesRead, NULL))
		{
			int err = GetLastError();
			if (err != ERROR_BROKEN_PIPE)
				cerr << "[rgat]Error: thread " << threadID << " pipe read ERROR: " << err << ". [Closing handler]" << endl;
			break;
		}

		if (bytesRead >= TAGCACHESIZE) {
			cerr << "\t\t[rgat](Easily fixable) Error: Excessive data sent to cache!" << endl;
			break;
		}

		tagReadBuf[bytesRead] = 0;
		if (tagReadBuf[bytesRead - 1] != '@')
		{
			die = true;
			if (!bytesRead) break;
			if (tagReadBuf.at(0) != 'X')
			{
				std::string bufstring(tagReadBuf.begin(), tagReadBuf.begin() + bytesRead);
				cerr << "[rgat]ERROR: [threadid" << threadID << "] Improperly terminated trace message recieved [" 
					<< bufstring << "]. (" << bytesRead << " bytes) Terminating." << endl;
			}
			
			break;
		}
		

		string *msgbuf = new string(tagReadBuf.begin(), tagReadBuf.begin() + bytesRead);

		add_message(msgbuf);
		++itemsRead;
	}

	pipeClosed = true;
	//wait until buffers emptied
	while (!firstQueue.empty() && !secondQueue.empty() && !die)
		Sleep(10);

	alive = false;
}
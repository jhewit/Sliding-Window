//------------------------------udphw3case4.cpp-------------------------------
//Author: Johnathan Hewit
//Description:
//----------------------------------------------------------------------------

#include <stdio.h>
#include <vector>
#include "UdpSocket.h"
#include "Timer.h"
#include <stdlib.h>
#include <time.h>

using namespace std;

//------------------------------clientStopWait-------------------------------
//Description:
//---------------------------------------------------------------------------
int clientStopWait(UdpSocket &sock, const int max, int message[])
{
    Timer timer;
    int count = 0;
    for (int i = 0; i < max; i++)
    {
        timer.start();
        message[0] = i;
        sock.sendTo((char *)message, sizeof(&message));
        while (sock.pollRecvFrom() < 1)
        {
            usleep(1);
            if (timer.lap() >= 1500)
            {
                cerr << "Error occurred. Retransmitting..." << endl;
                count++;
                break;
            }
        }
        if (timer.lap() >= 1500)
        {
            i--;
            continue;
        }
        sock.recvFrom((char *)message, sizeof(&message));

        cout << "Time: " << timer.lap() << endl;
    }
    return count;
} // end of clientStopWait

//------------------------------serverReliable-------------------------------
//Description:
//---------------------------------------------------------------------------
int serverReliable(UdpSocket &sock, const int max, int message[])
{
    cout << "Beginning the transmissions..." << endl;
    for (int i = 0; i < max; i++)
    {
        if (message[0] == (max - 1))
        {
            break;
        }
        sock.recvFrom((char *)message, sizeof(&message));
        if (message[0] == i)
        {
            cout << "Send ACK for message: " << message[0] << endl;
            sock.ackTo((char *)message, sizeof(&message));
        }
        else
        {
            while (sock.pollRecvFrom() < 1)
            {
                usleep(1);
            }
            i--;
            cout << "Go back..." << endl;
        }
    }
    return message[0];
} // end of serverReliable

//----------------------------clientSlidingWindow----------------------------
//Description:
//---------------------------------------------------------------------------
int clientSlidingWindow(UdpSocket &sock, const int max, int message[], int windowSize)
{
    cout << "Starting Sliding Window..." << endl;
    int count = 0;
    int i = 0;
    int unAck = 0;
    Timer timer;
    int lastAckFrame = 0;
    while (i < max)
    {
        while (unAck < windowSize)
        {
            message[0] = i;
            sock.sendTo((char *)message, sizeof(&message));
            i++;
            unAck++;
        }

        if (sock.pollRecvFrom() > 0)
        {
            sock.recvFrom((char *)message, sizeof(&message));
            unAck--;
        }
        else
        {
            {
                timer.start();
                while (sock.pollRecvFrom() < 1)
                {
                    usleep(1);
                    if (timer.lap() >= 1500)
                    {
                        cout << "Timeout occurred. Retransmitting..." << endl;
                        count ++;
                        break;
                    }
                }
                if (timer.lap() >= 1500)
                {
                    i--;
                    continue;
                }
            }
        }   
    }
    return count;
} // end of clientSlidingWindow

//------------------------------serverEarlyRetrans-----------------------------
//Description:
//---------------------------------------------------------------------------
void serverEarlyRetrans(UdpSocket &sock, const int max, int message[], int windowSize)
{
    cerr << "Beginning server early retransmission..." << endl;
    int lastFrameRec = 0;
    int lastSeqRec = -1;
    int lastAckFrame = 0;
    int window[windowSize];
    int randomNum;
    srand (time(NULL));
    for (int i = 0; i < windowSize; i++)
    {
        window[i] = -1;
    }
    while (lastFrameRec < max)
    {
        while (sock.pollRecvFrom() < 1)
        {
            usleep(1);
        }
        sock.recvFrom((char *)message, MSGSIZE);
        lastFrameRec = message[0];

        int index = lastFrameRec % windowSize;
        randomNum = rand() % 101;
        if (randomNum <= 10)
        {
            sock.ackTo((char *)&lastSeqRec, sizeof(int));
            cerr << "Dropping packet, requesting retransmission..." << endl;
            continue;
        }

        if (index < lastAckFrame || index >= windowSize)
        {
            sock.ackTo((char *)&lastSeqRec, sizeof(int));
            cerr << "Outside of window size:" << endl;
            continue;
        }
        else if (index == lastAckFrame)
        {
            window[index] = lastFrameRec;
            lastSeqRec = lastFrameRec;

            while (window[lastAckFrame] > -1)
            {
                lastSeqRec = window[lastAckFrame];
                window[lastAckFrame] = -1;
                lastAckFrame++;
                lastAckFrame %= windowSize;
                if (window[lastAckFrame] == -1)
                {
                    sock.ackTo((char *)&lastSeqRec, sizeof(int));
                }
            }
        }
        else
        {
            window[index] = lastFrameRec;
            sock.ackTo((char *)&lastSeqRec, sizeof(int));
        }
        if (lastFrameRec == (max - 1))
        {
            break;
        }
    }
} // end of serverEarlyRetrans
//--------------------------------udphw3.cpp----------------------------------
//Author: Johnathan Hewit
//Description: UWB Homework Assignment 3: A set of programs for creating a UDP
//             protocol and testing various methods and protocols of data
//             transfer from client to server.
//----------------------------------------------------------------------------

#include <stdio.h>
#include <vector>
#include "UdpSocket.h"
#include "Timer.h"

using namespace std;

//------------------------------clientStopWait-------------------------------
//Description: Function that receives a socket descriptor, a maximum size for
//             messages and the array for the messages. This function is a
//             client that writes its sequence number in a message to the server, 
//             serverReliable, and waits for a response up until a timeout is
//             reached, then it resends the message if the server has not responded. 
//             It returns a count of dropped packets based on a count of 
//             retransmissions.
//---------------------------------------------------------------------------
int clientStopWait(UdpSocket &sock, const int max, int message[])
{
    cerr << "Beginning client stop and wait transmissions..." << endl;
    Timer timer;
    int count = 0;
    for (int i = 0; i < max; i++)
    {
        timer.start();
        message[0] = i;
        sock.sendTo((char *)message, sizeof(&message)); // Transmit message to server
        while (sock.pollRecvFrom() < 1) // Wait for incoming message
        {
            usleep(1);
            if (timer.lap() >= 1500) // If timer goes off, resend and count occurrence
            {
                count++;
                break;
            }
        }
        if (timer.lap() >= 1500)
        {
            i--;
            continue;
        }
        sock.recvFrom((char *)message, sizeof(&message)); // Otherwise, the message arrives,
    }                                                     // receive it and go to the next
    return count;
} // end of clientStopWait

//------------------------------serverReliable-------------------------------
//Description: Function that receives a socket descriptor, a maximum size for
//             messages and the array for the messages. This function is a
//             server that receives transmissions from the clientStopWait
//             function and returns an ACK or acknowledgement as a response
//             if the sequence number is what it's expecting. Otherwise, it
//             allows the client to timeout.
//---------------------------------------------------------------------------
int serverReliable(UdpSocket &sock, const int max, int message[])
{
    cerr << "Beginning reliable server and receiving transmissions..." << endl;
    for (int i = 0; i < max; i++)
    {
        if (message[0] == (max - 1)) // Check if we are at the last sequence number
        {
            break;
        }
        sock.recvFrom((char *)message, sizeof(&message)); // Receive incoming message
        if (message[0] == i)
        {
            sock.ackTo((char *)message, sizeof(&message)); // Send ACK to client
        }
        else
        {
            while (sock.pollRecvFrom() < 1) // If not expected message, wait for another
            {                               // and roll back
                usleep(1);
            }
            i--;
        }
    }
    return message[0];
} // end of serverReliable

//----------------------------clientSlidingWindow----------------------------
//Description: Function that receives a socket descriptor, a maximum size for
//             messages in a frame, the array for the messages, and a window 
//             size for determining the number of frames in the sliding window.
//             The function is a client that fills the messages in each frame
//             of a window with sequence numbers and sends those to the
//             serverEarlyRetrans server. It then awaits a response in the form
//             of ACKs or acknowledgments and continues to the next sequence
//             numbers in the following window. If a timeout occurs, it
//             retransmits all messages up until the last ACK it received from
//             the server. It returns the number of dropped packets based on
//             a count of retransmissions.
//---------------------------------------------------------------------------
int clientSlidingWindow(UdpSocket &sock, const int max, int message[], int windowSize)
{
    cerr << "Beginning Sliding Window..." << endl;
    int count =         0;
    int seqNo =         0;
    int unAck =         0;
    Timer timer;
    int lastSeqRec =    0;
    int lastAckRec =   -1;

    while (seqNo < max)
    {
        while (unAck < windowSize)
        {
            if (seqNo == max) // We've reached the end and need to stop sending packets
            {
                break;
            }
            message[0] = seqNo;
            sock.sendTo((char *)message, sizeof(&message)); // Send the message
            seqNo++;
            unAck++;
        }

        timer.start();
        while (timer.lap() < 1500)
        {
            if (sock.pollRecvFrom() > 0) // Check if receiving a response
            {
                sock.recvFrom((char *)message, sizeof(&message)); // Record response
                lastSeqRec = message[0];

                if (lastSeqRec == (seqNo - 1)) // Received all packets in window
                {
                    unAck -= windowSize;        // Reset unAck tracker back to 0
                    lastAckRec = lastSeqRec;
                    break;
                }
                else // Some packets were lost, retransmit
                {
                    unAck -= ((seqNo - 1) - lastSeqRec);
                    seqNo = lastSeqRec + 1;
                    lastAckRec = lastSeqRec;
                    count++;
                    break;
                }
            }
        }
        if (timer.lap() >= 1500) // Handle timeout scenario
        {
            count++;
            unAck -= ((seqNo - 1)  - lastSeqRec);
            seqNo = lastSeqRec + 1;
            continue;
        }  
    }
    return count;
} // end of clientSlidingWindow

//------------------------------serverEarlyRetrans-----------------------------
//Description: Function that receives a socket descriptor, a maximum size for
//             messages in a frame, the array for the messages, and a window 
//             size for determining the number of frames in the sliding window.
//             The function is a server that waits for incoming messages, reads
//             the sequence number contained within it,  places it within a 
//             window, and returns a cumulative ACK to the client, 
//             clientSlidingWindow, if it receives the next expected and
//             in-order sequence number and the window is full. If it does not,
//             the server allows the client to time out forcing a resubmission
//             of the same message.
//---------------------------------------------------------------------------
void serverEarlyRetrans(UdpSocket &sock, const int max, int message[], int windowSize)
{
    cerr << "Beginning server early retransmission..." << endl;
    int currentFrame =  0;
    int currentSeqNo =  0;
    int lastFrameRec =  0;
    int lastSeqRec =   -1;
    int window[windowSize];
    for (int i = 0; i < windowSize; i++)
    {
        window[i] = -1;
    }
    while (currentSeqNo < max)
    {
        while (sock.pollRecvFrom() < 1) // Wait until a message comes in from the client
        {
            usleep(1);
        }
        sock.recvFrom((char *)message, sizeof(&message));
        lastSeqRec = message[0];
        lastFrameRec = currentSeqNo - 1;

        if (currentSeqNo == lastSeqRec)
        {
            window[currentFrame] = lastSeqRec;
            if (currentFrame == (windowSize - 1) || lastSeqRec == (max - 1)) // Last frame in window or last sequence in
            {                                                                // data packets, send cumulative ACK
                sock.ackTo((char *)&lastSeqRec, sizeof(int));
                currentSeqNo++;
                currentFrame = 0;
                continue;
            }
            else // Still more messages in the current window yet to be received
            {
                currentFrame++;
                currentSeqNo++;
            }
        }
        else // Received sequence number is out of order, do not send an ACK to force a timeout and retransmission
        {
            continue;
        }
    }
} // end of serverEarlyRetrans
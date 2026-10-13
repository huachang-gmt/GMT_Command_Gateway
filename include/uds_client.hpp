#ifndef UDS_CLIENT_HPP
#define UDS_CLIENT_HPP

enum class UdsSaveResult
{
    SAVE_OK,
    SAVE_ERROR,
    CONNECT_ERROR,
    SEND_ERROR,
    RECEIVE_ERROR,
    TIMEOUT,
    INVALID_RESPONSE
};

class UdsClient
{
public:
    UdsSaveResult saveTrajectory();
};

#endif // UDS_CLIENT_HPP

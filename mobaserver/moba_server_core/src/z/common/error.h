#ifndef Z_PUBLIC_ERRNO_H
#define Z_PUBLIC_ERRNO_H

namespace z {
    

class ZErrno 
{
public:
    static void Clear()
    {
        error_ = param1_ = param2_ = 0;
        msg_seq_ = msg_id_ = 0;
    }
    static void SetError(int32 error)
    {
        error_ = error;
    }
    static void SetError(int32 error, int64 param1)
    {
        error_ = error;
        param1_ = param1;
    }
    static void SetError(int32 error, int64 param1, int64 param2)
    {
        error_ = error;
        param1_ = param1;
        param2_ = param2;
    }
    static void SetPlayerAction(int64 player_id, int32 msg_id, int64 msg_seq)
    {
        player_id_ = player_id;
        msg_id_ = msg_id;
        msg_seq_ = msg_seq;
    }
    
    static uint32 error() {return error_;}
    static uint64 param1() {return param1_;}
    static uint64 param2() {return param2_;}

    static uint32 msg_id() {return msg_id_;}
    static uint64 player_id() {return player_id_;}
    static uint64 msg_seq() {return msg_seq_;}

private:
    static uint32 error_;
    static uint64 param1_;
    static uint64 param2_;
    static uint32 msg_id_;
    static uint64 msg_seq_;
    static uint64 player_id_;

};

}; //namespace z

#define CLEAR_ERROR() z::ZErrno::Clear()

#define SET_ERROR z::ZErrno::SetError
#define GET_ERROR() z::ZErrno::error()
#define GET_ERROR_PARAM1() z::ZErrno::param1()
#define GET_ERROR_PARAM2() z::ZErrno::param2()

#define GET_MSG_PLAYER() z::ZErrno::player_id()
#define GET_MSG() z::ZErrno::msg_id()
#define GET_MSG_SEQ() z::ZErrno::msg_seq()





#endif // Z_PUBLIC_ERRNO_H

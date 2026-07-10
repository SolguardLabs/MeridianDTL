#ifndef MERIDIAN_STATUS_H
#define MERIDIAN_STATUS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum MdStatus {
    MD_OK = 0,
    MD_ERR_ARGUMENT = 1,
    MD_ERR_CAPACITY = 2,
    MD_ERR_NOT_FOUND = 3,
    MD_ERR_DUPLICATE = 4,
    MD_ERR_SIGNATURE = 5,
    MD_ERR_BALANCE = 6,
    MD_ERR_POLICY = 7,
    MD_ERR_STATE = 8,
    MD_ERR_EXPIRED = 9,
    MD_ERR_INTERNAL = 10
} MdStatus;

const char *md_status_name(MdStatus status);

#ifdef __cplusplus
}
#endif

#endif

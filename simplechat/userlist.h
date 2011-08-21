
#if !defined(samplechat_userlist_h)
#define samplechat_userlist_h

struct UserInfo;

bool load_userlist();
bool save_userlist();
unsigned int count_users();
void get_user_by_index(unsigned int index, UserInfo &ui);
bool get_user_by_name(char const *name, UserInfo &ui);
bool update_user_by_index(unsigned int index, UserInfo const &ui);
void delete_user_by_index(unsigned int index);
bool new_user(UserInfo const &ui);

#endif  //  samplechat_userlist_h

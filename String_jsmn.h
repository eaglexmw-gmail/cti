#ifndef _CTI_STRING_JSMN_H_
#define _CTI_STRING_JSMN_H_

extern int String_eq_jsmn(String * json_text, jsmntok_t token, const char *target);
extern String * String_dup_jsmn(String * json_text, jsmntok_t token);
extern void jsmn_dump(jsmntok_t * tokens, int num_tokens, int limit);

#endif

// Data and function declarations from https://github.com/datadiode/Remimu
// SPDX-Licence-Identifier: CC0-1.0

// Compensate for lack of <stdint.h> in vc9
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned __int64 uint64_t;
typedef short int16_t;
typedef int int32_t;
typedef __int64 int64_t;

typedef struct _RegexToken {
    uint8_t kind;
    uint8_t mode;
    uint16_t count_lo;
    uint16_t count_hi; // 0 means no limit
    uint16_t mask[16]; // for groups: mask 0 stores group-with-quantifier number (quantifiers are +, *, ?, {n}, {n,}, or {n,m})
    int16_t pair_offset; // from ( or ), offset in token list to matching paren. TODO: move into mask maybe
} RegexToken;

/// Returns a negative number on failure:
/// -1: Regex string is invalid or using unsupported features or too long.
/// -2: Provided buffer not long enough. Give up, or reallocate with more length and retry.
/// Returns 0 on success.
/// On call, token_count pointer must point to the number of tokens that can be written to the tokens buffer.
/// On successful return, the number of actually used tokens is written to token_count.
/// Sets token_count to zero if a regex is not created but no error happened (e.g. empty pattern).
/// Flags: Not yet used.
/// SAFETY: Pattern must be null-terminated.
/// SAFETY: tokens buffer must have at least the input token_count number of RegexToken objects. They are allowed to be uninitialized.
int regex_parse(const char * pattern, RegexToken * tokens, int16_t * token_count, int32_t flags);

// Returns match length if text starts with a regex match.
// Returns -1 if the text doesn't start with a regex match.
// Returns -2 if the matcher ran out of memory or the regex is too complex.
// Returns -3 if the regex is somehow invalid.
// The first cap_slots capture positions and spans (lengths) will be written to cap_pos and cap_span. If zero, will not be written to.
// SAFETY: The text variable must be null-terminated, and start_i must be the index of a character within the string or its null terminator.
// SAFETY: Tokens array must be terminated by a REMIMU_KIND_END token (done by default by regex_parse).
// SAFETY: Partial capture data may be written even if the match fails.
int64_t regex_match(const RegexToken * tokens, const char * text, size_t start_i, uint16_t cap_slots, int64_t * cap_pos, int64_t * cap_span);

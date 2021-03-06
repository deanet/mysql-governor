/* Copyright Cloud Linux Inc 2010-2011 All Rights Reserved
 *
 * Licensed under CLOUD LINUX LICENSE AGREEMENT
 * http://cloudlinux.com/docs/LICENSE.TXT
 *
 * db_governor application
 * author Igor Seletskiy <iseletsk@cloudlinux.com>
 * author Alexey Berezhok <alexey.berezhok@cloudlinux.com>
 *
 */

#ifndef LOG_DECODER_H_
#define LOG_DECODER_H_

long
getLimitValuePeriod (Account * ac, T_LONG lm);
long long
getRestrictValue (Account * ac);
long
getLimitValue (Account * ac, stats_limit_cfg * lm);
long long
getLongRestrictValue (Account * ac);
long long
getMidRestrictValue (Account * ac);
long long
getShortRestrictValue (Account * ac);
long long
getCurrentRestrictValue (Account * ac);
void
prepareRestrictDescription (char *buffer, Account * ac,
			    stats_limit_cfg * limit);
void getPeriodName(char *ch, Account * ac);
stats_limit *
getRestrictDump(Account * ac);

#endif /* LOG_DECODER_H_ */

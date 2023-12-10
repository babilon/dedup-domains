/**
 * matchstrength.h
 *
 * Part of pfb_dnsbl_prune
 *
 * Copyright (c) 2023 robert.babilon@gmail.com
 * All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef MATCH_STRENGTH_H
#define MATCH_STRENGTH_H

typedef enum MatchStrength
{
	MATCH_BOGUS = -2,
	MATCH_NOTSET = -1,
	MATCH_WEAK = 0,
	MATCH_FULL = 1,
	MATCH_REGEX = 2
} MatchStrength_t;

#endif

/**
 * codecoverage.h
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
#ifndef CODE_COVERAGE_H
#define CODE_COVERAGE_H

#ifdef CODECOVERAGE
extern void add_coverage(unsigned long, const char *filename);
#define ADD_CC do { add_coverage(__LINE__, __FILE__); } while(0)
#define ADD_TCC do { add_coverage(__LINE__, __FILE__); } while(0)
#else
#define ADD_CC do {} while(0)
#define ADD_TCC do {} while(0)
#endif

#ifdef REGEX_ENABLED
#define ADD_CC_REGEX ADD_CC
#else
#define ADD_CC_SINGLE ADD_CC
#endif

extern void print_lineshit();

#endif

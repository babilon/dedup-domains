/**
 * test.h
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
#ifndef TEST_H
#define TEST_H

#ifdef BUILD_TESTS
extern void info_csvline();
extern void info_domain();
extern void info_DomainTree();
extern void info_pfb_prune();

extern void test_csvline();
extern void test_domain();
extern void test_DomainTree();
extern void test_pfb_prune();
extern void test_rw_pfb_csv();
extern void test_input_args();
extern void test_carry_over();
#endif

#endif

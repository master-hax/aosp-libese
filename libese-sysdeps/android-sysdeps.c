/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void ese_abort(int code, const char *message) { abort(); }

void *ese_memcpy(void *__dest, const void *__src, uint64_t __n) {
  return memcpy(__dest, __src, __n);
}

void *ese_memset(void *__s, int __c, uint64_t __n) {
  return memset(__s, __c, __n);
}

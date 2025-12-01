#include "config.h" // IWYU pragma: keep

#include "CwdUtils.h"

#include <stdbool.h>
#include <stdlib.h>
#include <wchar.h>

#include "XUtils.h"


typedef struct ShortenCwdContext {
   size_t maxLength;
   size_t len;
   wchar_t **parts;
   size_t partsLen;
   size_t *partLengths;
} ShortenCwdContext;

static void ShortenCwdContext_init(ShortenCwdContext *ctx, wchar_t *cwd, size_t maxLength) {
   ctx->maxLength = maxLength;
   ctx->len = wcslen(cwd);
   ctx->parts = Wstring_split(cwd, L'/', &ctx->partsLen);
   ctx->partLengths = xCalloc(ctx->partsLen, sizeof(size_t));
   for (size_t i = 0; i < ctx->partsLen; i++)
      ctx->partLengths[i] = wcslen(ctx->parts[i]);
}

static void ShortenCwdContext_destroy(ShortenCwdContext *ctx) {
   for (size_t i = 0; i < ctx->partsLen; i++)
      free(ctx->parts[i]);
   free(ctx->parts);
   free(ctx->partLengths);
   *ctx = (ShortenCwdContext){0};
}

static void shortenCwdParts(ShortenCwdContext *ctx) {
   for (size_t i = ctx->partsLen - 2; i != (size_t)-1 && ctx->len > ctx->maxLength; i--) {
      if (ctx->partLengths[i] < 3)
         continue;

      size_t extraChars = ctx->len - ctx->maxLength;
      size_t maxRemovableChars = ctx->partLengths[i] - 2;
      size_t charsToRemove = extraChars < maxRemovableChars ? extraChars : maxRemovableChars;

      ctx->partLengths[i] -= charsToRemove;
      ctx->len -= charsToRemove;

      Wstring_safeWcsncpy(ctx->parts[i] + (ctx->partLengths[i] - 1), L"~", 2);
   }
}

static size_t collapseCwdParts(ShortenCwdContext *ctx, bool doActualWork) {
   if (ctx->len <= ctx->maxLength || ctx->partsLen <= 3)
      return 0;

   size_t len = ctx->len;

   size_t i;
   for (i = ctx->partsLen - 2; i > 1; i--) {
      if (len + (3 - ctx->partLengths[i]) <= ctx->maxLength)
         break;

      len -= ctx->partLengths[i] + 1;

      if (doActualWork) {
         ctx->partLengths[i] = 0;
         free(ctx->parts[i]);
         ctx->parts[i] = NULL;
      }
   }

   len += 3 - ctx->partLengths[i];
   size_t diff = ctx->len - len;

   if (doActualWork) {
      wchar_t newPart[] = L"~~~";
      newPart[0] = ctx->parts[i][0];
      free(ctx->parts[i]);
      ctx->parts[i] = xWcsdup(newPart);
      ctx->partLengths[i] = 3;
      ctx->len = len;
   }

   return diff;
}

static size_t shortenCwdLastPart(ShortenCwdContext *ctx, bool doActualWork) {
   if (ctx->len <= ctx->maxLength)
      return 0;

   size_t lastPartLen = ctx->partLengths[ctx->partsLen - 1];
   if (lastPartLen <= 3)
      return 0;

   wchar_t *lastPart = ctx->parts[ctx->partsLen - 1];
   size_t extraChars = ctx->len - ctx->maxLength + 1;
   size_t maxRemovableChars = lastPartLen - 2;
   size_t charsToRemove = extraChars < maxRemovableChars ? extraChars : maxRemovableChars;

   if (doActualWork) {
      size_t charsAtBeginning = (lastPartLen - charsToRemove + 1) / 2;
      size_t charsAtEnd = lastPartLen - charsToRemove - charsAtBeginning;
      lastPart[charsAtBeginning] = '~';
      wmemmove(lastPart + charsAtBeginning + 1, lastPart + lastPartLen - charsAtEnd, charsAtEnd);
      lastPart[charsAtBeginning + charsAtEnd + 1] = '\0';
      ctx->partLengths[ctx->partsLen - 1] = lastPartLen - charsToRemove + 1;
      ctx->len -= charsToRemove - 1;
   }

   return charsToRemove - 1;
}

static wchar_t* buildCwdFromParts(ShortenCwdContext *ctx) {
   size_t len = ctx->partsLen - 1;
   for (size_t i = 0; i < ctx->partsLen; i++)
      len += ctx->partLengths[i];

   wchar_t *newCwd = xCalloc(len + 1, sizeof(wchar_t));

   newCwd[0] = '\0';
   for (size_t i = 0, writeIndex = 0; i < ctx->partsLen; i++) {
      if (!ctx->parts[i])
         continue;

      Wstring_safeWcsncpy(newCwd + writeIndex, ctx->parts[i], ctx->partLengths[i] + 1);
      writeIndex += ctx->partLengths[i];
      if (i < ctx->partsLen - 1)
         newCwd[writeIndex++] = L'/';
   }

   return newCwd;
}

char* CwdUtils_shortenCwd(const char *cwd, const size_t maxLength) {
   wchar_t *wcwd = xMbstowcs(cwd);
   size_t len = wcslen(wcwd);
   if (len <= maxLength) {
      free(wcwd);
      return xStrdup(cwd);
   }

   ShortenCwdContext ctx;
   ShortenCwdContext_init(&ctx, wcwd, maxLength);
   free(wcwd);
   wcwd = NULL;

   shortenCwdParts(&ctx);
   if (shortenCwdLastPart(&ctx, false) > collapseCwdParts(&ctx, false)) {
      shortenCwdLastPart(&ctx, true);
      collapseCwdParts(&ctx, true);
   } else {
      collapseCwdParts(&ctx, true);
      shortenCwdLastPart(&ctx, true);
   }

   wchar_t *newWcwd = buildCwdFromParts(&ctx);
   char *newCwd = xWcstombs(newWcwd);
   free(newWcwd);

   ShortenCwdContext_destroy(&ctx);

   return newCwd;
}

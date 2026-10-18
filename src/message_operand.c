// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "message_operand.h"

#include <string.h>

// A date value as a function receives it: a zoned one in the timeZone
// option's zone, else its operand's.
static void DateOperandOf(const DateValue* d, mlocMessageOperand* out, mlocMessageResolved* storage)
{
    uint8_t has = d->has & (dateHasDate | dateHasTime);
    out->kind = has == dateHasDate   ? mloc_messageOperandDate
                : has == dateHasTime ? mloc_messageOperandTime
                                     : mloc_messageOperandDateTime;
    storage->zoned.dateTime = d->value;
    out->dateTime = &storage->zoned.dateTime;
    if ((d->has & dateHasZone) != 0)
    {
        storage->zoned.instant = d->instant;
        storage->zoned.zone = d->zoneMode == dateZoneGiven ? d->timeZone : d->zone;
        if (mlocGetDateTimeFor(&storage->zoned.zone, &d->instant, mloc_calendarIso8601,
                               &storage->zoned.dateTime) != mloc_success)
        {
            storage->zoned.dateTime = d->value;
        }
        out->instant = &storage->zoned.instant;
        out->zone = &storage->zoned.zone;
    }
}

// A value as a function receives it; storage holds a resolved value.
void mlocToOperand(const Value* v, mlocMessageOperand* out, mlocMessageResolved* storage)
{
    memset(out, 0, sizeof *out);
    ValueKind kind = v->kind == valueString ? v->behind : v->kind;
    if (kind == valueText)
    {
        out->kind = mloc_messageOperandText;
        out->text = (const char*)v->text;
        out->length = v->length;
    }
    else if (kind == valueArgument)
    {
        out->kind = mloc_messageOperandArgument;
        out->argument = v->argument;
    }
    else if (kind == valueResolved)
    {
        *storage = v->resolved;
        out->kind = mloc_messageOperandResolved;
        out->function = v->function;
        out->resolved = storage;
    }
    else if (kind == valueNumber && v->number.function == numberFunctionUnit)
    {
        storage->decimal = v->number.decimal;
        out->kind = mloc_messageOperandUnit;
        out->number = &storage->decimal;
        out->text = v->number.unit;
        out->length = v->number.unitLength;
    }
    else if (kind == valueNumber && v->number.function == numberFunctionCurrency)
    {
        storage->currency.amount = v->number.decimal;
        memcpy(storage->currency.code, v->number.code, 3);
        storage->currency.code[3] = 0;
        out->kind = mloc_messageOperandCurrency;
        out->number = &storage->currency.amount;
        out->text = storage->currency.code;
        out->length = 3;
    }
    else if (kind == valueNumber)
    {
        storage->decimal = v->number.decimal;
        out->kind = mloc_messageOperandNumber;
        out->number = &storage->decimal;
    }
    else if (kind == valueDate)
    {
        DateOperandOf(&v->date, out, storage);
    }
    else if (kind == valueDuration)
    {
        storage->duration = v->duration.values;
        out->kind = mloc_messageOperandDuration;
        out->duration = &storage->duration;
    }
    else
    {
        out->kind = mloc_messageOperandFallback;
    }
}

mlocMessageFaultKind mlocSetNumberFrom(void* context, const uint8_t* name, size_t length,
                                       const NumberOption* option)
{
    NumberSetting* s = context;
    if (option == NULL)
    {
        // A select that failed still counts as set from a variable.
        s->value->selectable =
            s->value->selectable && !(length == 6 && memcmp(name, "select", 6) == 0);
        return mloc_messageFaultNone;
    }
    return mlocSetNumberOption(s->value, s->function, name, length, option);
}

// The date value a date function's operand gives: a date/time literal
// (or a string argument that is one), a date, time or date-time
// argument, or another date function's value, whose override options
// pass on; false for anything else or parts the function cannot take.
bool mlocDateOperand(const mlocDataSet* data, DateFunction function, const Value* operand,
                     DateValue* out)
{
    if (operand == NULL)
    {
        return false;
    }
    ValueKind kind = operand->kind == valueString ? operand->behind : operand->kind;
    if (kind == valueDate)
    {
        *out = mlocDateFromValue(function, &operand->date);
        return mlocDateTakes(function, out->has);
    }
    DateMoment moment;
    bool read =
        (kind == valueText && mlocDateLiteral(operand->text, operand->length, data, &moment)) ||
        (kind == valueArgument && mlocArgumentDateTime(operand->argument, data, &moment));
    if (!read)
    {
        return false;
    }
    *out = mlocMakeDateValue(function, &moment);
    return mlocDateTakes(function, moment.has);
}

mlocMessageFaultKind mlocSetDateFrom(void* context, const uint8_t* name, size_t length,
                                     const NumberOption* option)
{
    DateSetting* s = context;
    return option == NULL ? mloc_messageFaultNone
                          : mlocSetDateOption(s->value, s->function, name, length, option, s->data);
}

mlocMessageFaultKind mlocSetDurationFrom(void* context, const uint8_t* name, size_t length,
                                         const NumberOption* option)
{
    return option == NULL ? mloc_messageFaultNone
                          : mlocSetDurationOption(context, name, length, option);
}

// The duration a mloc:duration operand gives: a duration literal (or a
// string argument that is one), a duration argument, a number of
// seconds (a number literal, a numeric argument, a number value), or
// another mloc:duration value, whose options pass on; false for anything
// else.
bool mlocDurationOperand(const Value* operand, DurationValue* out)
{
    ValueKind kind = operand == NULL                ? valueResolved
                     : operand->kind == valueString ? operand->behind
                                                    : operand->kind;
    mlocDurationValues values;
    mlocDecimal seconds;
    if (kind == valueDuration)
    {
        *out = operand->duration;
        return true;
    }
    bool read = (kind == valueText &&
                 (mlocDurationLiteral(operand->text, operand->length, &values) ||
                  (mlocNumberLiteral(operand->text, operand->length) &&
                   mlocParseDecimal((const char*)operand->text, operand->length, &seconds).status ==
                       mloc_success &&
                   mlocDurationFromSeconds(&seconds, &values)))) ||
                (kind == valueArgument && operand->argument->kind == mloc_messageValueDuration &&
                 (values = *operand->argument->as.duration, true)) ||
                (kind == valueArgument && mlocArgumentDecimal(operand->argument, &seconds) &&
                 mlocDurationFromSeconds(&seconds, &values)) ||
                (kind == valueNumber && mlocDurationFromSeconds(&operand->number.decimal, &values));
    if (read)
    {
        *out = mlocMakeDurationValue(&values);
    }
    return read;
}

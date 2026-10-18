#pragma once
#include "learning.h"
namespace wm {
enum class AnswerAction { Grade, Previous, Next, Skip };
inline AnswerAction AnswerInput(unsigned page,unsigned pages,Button key){
    if(page+1>=pages)return AnswerAction::Grade;
    if(key==Button::Ok)return AnswerAction::Next;
    if(key==Button::Up)return AnswerAction::Previous;
    return AnswerAction::Skip;
}
}

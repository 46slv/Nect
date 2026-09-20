#pragma once
#include "nect/core.hpp"
#include <functional>

namespace nect {
struct ExpressionProgram;
// Compiled data is transient and never part of a Scalar, native file or History.
struct CompiledExpression {
    std::shared_ptr<const ExpressionProgram> program;
};
CompiledExpression compile_expression(const Expression& expression);
void validate_expression_unit(const CompiledExpression&,const std::string& expected_unit);
const std::vector<Ref>& expression_dependencies(const CompiledExpression&);
double evaluate_expression(const CompiledExpression&,const std::string& expected_unit,
    const std::function<double(const Ref&)>& resolve);
}

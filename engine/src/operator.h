/* Copyright (C) 2003-2015 LiveCode Ltd.

This file is part of LiveCode.

LiveCode is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation.

LiveCode is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with LiveCode.  If not see <http://www.gnu.org/licenses/>.  */

//
// Operator class declarations
//
#ifndef	OPERATORS_H
#define	OPERATORS_H

#include "exec.h"
#include "executionerrors.h"

#include "express.h"
#include "hxtast.h"     // HXTExprType, kHXTExpr_* — needed by hxt_expr_type() overrides

class MCChunk;

//////////

class MCUnaryOperator: public MCExpression
{
public:
    // HXT: single operand is in MCExpression::right.
    virtual HXTExprType hxt_expr_type() const { return kHXTExpr_Null; }
    virtual bool hxt_serialize(MCHXTASTWriter &w) const override;
    virtual bool hxt_deserialize_body(MCHXTASTReader &r) override;
};

class MCBinaryOperator: public MCExpression
{
public:
    // HXT: left operand in MCExpression::left, right in MCExpression::right.
    virtual HXTExprType hxt_expr_type() const { return kHXTExpr_Null; }
    virtual bool hxt_serialize(MCHXTASTWriter &w) const override;
    virtual bool hxt_deserialize_body(MCHXTASTReader &r) override;
};

class MCMultiBinaryOperator: public MCExpression
{
public:
	virtual bool canbeunary(void) const {return false;}
    // HXT: left in MCExpression::left, right in MCExpression::right.
    // MCMinus overrides this to handle the unary case.
    virtual HXTExprType hxt_expr_type() const { return kHXTExpr_Null; }
    virtual bool hxt_serialize(MCHXTASTWriter &w) const override;
    virtual bool hxt_deserialize_body(MCHXTASTReader &r) override;
};

//////////

template <typename ParamType,
          void (*EvalMethod)(MCExecContext&, typename MCExecValueTraits<ParamType>::in_type, typename MCExecValueTraits<ParamType>::out_type),
          Exec_errors EvalError,
          Factor_rank Rank>
class MCUnaryOperatorCtxt: public MCUnaryOperator
{
public:
    MCUnaryOperatorCtxt()
    {
        rank = Rank;
    }

    virtual void eval_ctxt(MCExecContext &ctxt, MCExecValue &r_value)
    {
        ParamType t_right;
        ParamType t_result;

        if (!MCExecValueTraits<ParamType>::eval(ctxt, right, EvalError, t_right))
            return;

        EvalMethod(ctxt, t_right, t_result);

        MCExecValueTraits<ParamType>::release(t_right);

        if (!ctxt . HasError())
            MCExecValueTraits<ParamType>::set(r_value, t_result);
    }
};

template<typename ParamType,
         typename ReturnType,
         void (*EvalMethod)(MCExecContext&, typename MCExecValueTraits<ParamType>::in_type, typename MCExecValueTraits<ParamType>::in_type, typename MCExecValueTraits<ReturnType>::out_type),
         Exec_errors EvalLeftError,
         Exec_errors EvalRightError,
         Factor_rank Rank>
class MCBinaryOperatorCtxt: public MCBinaryOperator
{
public:
    MCBinaryOperatorCtxt()
    {
        rank = Rank;
    }

    virtual void eval_ctxt(MCExecContext &ctxt, MCExecValue &r_value)
    {
        ParamType t_right;
        ParamType t_left;
        ReturnType t_result;

        if (!MCExecValueTraits<ParamType>::eval(ctxt, left, EvalLeftError, t_left))
                return;

        if (!MCExecValueTraits<ParamType>::eval(ctxt, right, EvalRightError, t_right))
        {
            MCExecValueTraits<ParamType>::release(t_left);
            return;
        }

        EvalMethod(ctxt, t_left, t_right, t_result);

        MCExecValueTraits<ParamType>::release(t_left);
        MCExecValueTraits<ParamType>::release(t_right);

        if (!ctxt . HasError())
            MCExecValueTraits<ReturnType>::set(r_value, t_result);
    }
};

template<void (*Eval)(MCExecContext&, real64_t, real64_t, real64_t&),
         void (*EvalArrayByNumber)(MCExecContext&, MCArrayRef, real64_t, MCArrayRef&),
         void (*EvalArrayByArray)(MCExecContext&, MCArrayRef, MCArrayRef, MCArrayRef&),
         Exec_errors EvalLeftError,
         Exec_errors EvalRightError,
         Exec_errors MismatchError,
         bool CanBeUnary,
         Factor_rank Rank>
class MCMultiBinaryOperatorCtxt: public MCMultiBinaryOperator
{
public:
    MCMultiBinaryOperatorCtxt()
    {
        rank = Rank;
    }

    virtual void eval_ctxt(MCExecContext &ctxt, MCExecValue &r_value)
    {
        MCExecValue t_left, t_right;
        t_left . type = kMCExecValueTypeNone;
        t_right . type = kMCExecValueTypeNone;

        left -> eval_ctxt(ctxt, t_left);
        if (ctxt . HasError()
                || ! ctxt . ConvertToNumberOrArray(t_left))
        {
            ctxt . LegacyThrow(EvalLeftError);
            return;
        }

        right -> eval_ctxt(ctxt, t_right);
        if (ctxt . HasError()
                || !ctxt . ConvertToNumberOrArray(t_right))
        {
            ctxt . LegacyThrow(EvalRightError);
            if (t_left . type == kMCExecValueTypeArrayRef)
                MCValueRelease(t_left . arrayref_value);
            return;
        }

        r_value . valueref_value = nil;
        if (t_left . type == kMCExecValueTypeArrayRef)
        {
            if (t_right . type == kMCExecValueTypeArrayRef)
                EvalArrayByArray(ctxt, t_left . arrayref_value, t_right . arrayref_value, r_value . arrayref_value);
            else
                EvalArrayByNumber(ctxt, t_left . arrayref_value, t_right . double_value, r_value . arrayref_value);
        }
        else
        {
            if (t_right . type == kMCExecValueTypeArrayRef)
                ctxt . LegacyThrow(MismatchError);
            else
                Eval(ctxt, t_left . double_value, t_right . double_value, r_value . double_value);
        }
        
        if (!ctxt . HasError())
        {
            r_value . type = t_left . type;
        }

        if (t_left . type == kMCExecValueTypeArrayRef)
            MCValueRelease(t_left . arrayref_value);
        if (t_right . type == kMCExecValueTypeArrayRef)
            MCValueRelease(t_right . arrayref_value);
    }

    virtual bool canbeunary() const { return CanBeUnary; }
};

template<void (*Eval)(MCExecContext&, real64_t, real64_t, real64_t&),
         void (*EvalArrayByNumber)(MCExecContext&, MCArrayRef, real64_t, MCArrayRef&),
         void (*EvalArrayByArray)(MCExecContext&, MCArrayRef, MCArrayRef, MCArrayRef&),
         Exec_errors EvalLeftError,
         Exec_errors EvalRightError,
         bool CanBeUnary,
         Factor_rank Rank>
class MCMultiBinaryCommutativeOperatorCtxt: public MCMultiBinaryOperator
{
public:
    MCMultiBinaryCommutativeOperatorCtxt()
    {
        rank = Rank;
    }

    virtual void eval_ctxt(MCExecContext &ctxt, MCExecValue &r_value)
    {
        MCExecValue t_left, t_right;
		t_left . type = kMCExecValueTypeNone;
		t_right . type = kMCExecValueTypeNone;

		// If the operator is unary, make sure that the LHS is initialised
        if (rank == FR_UNARY)
            MCExecValueTraits<double>::set(t_left, 0.0);

        // PM-2015-01-15: [[ Bug 14384 ]] Prevent a crash when putting +1 into variable
        if ((left != nil && (left -> eval_ctxt(ctxt, t_left), ctxt . HasError()))
                || !ctxt . ConvertToNumberOrArray(t_left))
        {
            ctxt . LegacyThrow(EvalLeftError);
            return;
        }

        if ((right != nil && (right -> eval_ctxt(ctxt, t_right), ctxt . HasError()))
                || !ctxt . ConvertToNumberOrArray(t_right))
        {
            ctxt . LegacyThrow(EvalRightError);
            if (t_left . type == kMCExecValueTypeArrayRef)
                MCValueRelease(t_left . valueref_value);
            return;
        }

        r_value . valueref_value = nil;
        if (t_left . type == kMCExecValueTypeArrayRef)
        {
            if (t_right . type == kMCExecValueTypeArrayRef)
                EvalArrayByArray(ctxt, t_left . arrayref_value, t_right . arrayref_value, r_value . arrayref_value);
            else
                EvalArrayByNumber(ctxt, t_left . arrayref_value, t_right . double_value, r_value . arrayref_value);
        }
        else
        {
            if (t_right . type == kMCExecValueTypeArrayRef)
                EvalArrayByNumber(ctxt, t_right . arrayref_value, t_left . double_value, r_value . arrayref_value);
            else
                Eval(ctxt, t_left . double_value, t_right . double_value, r_value . double_value);
        }
        
        if (!ctxt . HasError())
        {
            if (t_left . type == kMCExecValueTypeDouble && t_right . type == kMCExecValueTypeDouble)
                r_value . type = kMCExecValueTypeDouble;
            else
                r_value . type = kMCExecValueTypeArrayRef;
        }

        if (t_left . type == kMCExecValueTypeArrayRef)
            MCValueRelease(t_left . valueref_value);
        if (t_right . type == kMCExecValueTypeArrayRef)
            MCValueRelease(t_right . valueref_value);
        
    }

    virtual bool canbeunary() const { return CanBeUnary; }
};



//////////

class MCAnd : public MCExpression
{
public:
    MCAnd()
    {
        rank = FR_AND;
    }
    virtual void eval_ctxt(MCExecContext &, MCExecValue &r_value);
    virtual bool hxt_serialize(MCHXTASTWriter &w) const override;
    virtual bool hxt_deserialize_body(MCHXTASTReader &r) override;
};

class MCAndBits : public MCBinaryOperatorCtxt<uinteger_t, uinteger_t, MCMathEvalBitwiseAnd, EE_ANDBITS_BADLEFT, EE_ANDBITS_BADRIGHT, FR_AND_BITS>
{};

class MCConcat : public MCExpression
{
public:
    virtual bool hxt_serialize(MCHXTASTWriter &w) const override;
    virtual bool hxt_deserialize_body(MCHXTASTReader &r) override;
	MCConcat()
	{
		rank = FR_CONCAT;
    }
    virtual void eval_ctxt(MCExecContext &ctxt, MCExecValue &r_value);
};

class MCConcatSpace : public MCBinaryOperatorCtxt<MCStringRef, MCStringRef, MCStringsEvalConcatenateWithSpace, EE_CONCATSPACE_BADLEFT, EE_CONCATSPACE_BADRIGHT, FR_CONCAT>
{ public: HXTExprType hxt_expr_type() const override { return kHXTExpr_ConcatSp; } };

class MCContains : public MCBinaryOperatorCtxt<MCStringRef, bool, MCStringsEvalContains, EE_CONTAINS_BADLEFT, EE_CONTAINS_BADRIGHT, FR_COMPARISON>
{ public: HXTExprType hxt_expr_type() const override { return kHXTExpr_Contains; } };

class MCDiv : public MCMultiBinaryOperatorCtxt<
        MCMathEvalDiv,
        MCMathEvalDivArrayByNumber,
        MCMathEvalDivArrayByArray,
        EE_DIV_BADLEFT,
        EE_DIV_BADRIGHT,
        EE_DIV_MISMATCH,
        false,
        FR_MULDIV>
{ public: HXTExprType hxt_expr_type() const override { return kHXTExpr_IntDiv; } };

class MCEqual : public MCBinaryOperatorCtxt<MCValueRef, bool, MCLogicEvalIsEqualTo, EE_FACTOR_BADLEFT, EE_FACTOR_BADRIGHT, FR_EQUAL>
{ public: HXTExprType hxt_expr_type() const override { return kHXTExpr_Equal; } };

class MCGreaterThan : public MCBinaryOperatorCtxt<MCValueRef, bool, MCLogicEvalIsGreaterThan, EE_FACTOR_BADLEFT, EE_FACTOR_BADRIGHT, FR_COMPARISON>
{ public: HXTExprType hxt_expr_type() const override { return kHXTExpr_Greater; } };

class MCGreaterThanEqual : public MCBinaryOperatorCtxt<MCValueRef, bool, MCLogicEvalIsGreaterThanOrEqualTo, EE_FACTOR_BADLEFT, EE_FACTOR_BADRIGHT, FR_COMPARISON>
{ public: HXTExprType hxt_expr_type() const override { return kHXTExpr_GreaterEq; } };

class MCGrouping : public MCExpression
{
public:
	MCGrouping()
	{
		rank = FR_GROUPING;
    }
    virtual void eval_ctxt(MCExecContext &ctxt, MCExecValue &r_value);
};

class MCIs : public MCExpression
{
	Is_type form;
	Is_validation valid;
	Chunk_term delimiter;
public:
	MCIs()
	{
		rank = FR_EQUAL;
		form = IT_NORMAL;
		valid = IV_UNDEFINED;
		delimiter = CT_UNDEFINED;
	}
    Parse_stat parse(MCScriptPoint &, Boolean the);
    virtual void eval_ctxt(MCExecContext &ctxt, MCExecValue &r_value);
    // HXT: stores form(u8) + valid(u8) + delimiter(u8) + left + right.
    virtual bool hxt_serialize(MCHXTASTWriter &w) const override;
    virtual bool hxt_deserialize_body(MCHXTASTReader &r) override;
};

class MCItem : public MCBinaryOperatorCtxt<MCStringRef, MCStringRef, MCStringsEvalConcatenateWithComma, EE_CONCAT_BADLEFT, EE_CONCAT_BADRIGHT, FR_CONCAT>
{};

class MCLessThan : public MCBinaryOperatorCtxt<MCValueRef, bool, MCLogicEvalIsLessThan, EE_FACTOR_BADLEFT, EE_FACTOR_BADRIGHT, FR_COMPARISON>
{ public: HXTExprType hxt_expr_type() const override { return kHXTExpr_Less; } };

class MCLessThanEqual : public MCBinaryOperatorCtxt<MCValueRef, bool, MCLogicEvalIsLessThanOrEqualTo, EE_FACTOR_BADLEFT, EE_FACTOR_BADRIGHT, FR_COMPARISON>
{ public: HXTExprType hxt_expr_type() const override { return kHXTExpr_LessEq; } };

class MCMinus : public MCMultiBinaryOperator
{
public:
    MCMinus()
    {
        rank = FR_ADDSUB;
    }

    virtual void eval_ctxt(MCExecContext &ctxt, MCExecValue &r_value);

    virtual bool canbeunary(void) const {return true;}

    HXTExprType hxt_expr_type() const override { return kHXTExpr_Sub; }
    // HXT: writes kHXTExpr_Sub (binary) or kHXTExpr_Negate (unary, left==null).
    virtual bool hxt_serialize(MCHXTASTWriter &w) const override;
    // No hxt_deserialize_body override: MCMultiBinaryOperator reads left+right;
    // for the unary (kHXTExpr_Negate) case the factory writes right only and
    // sets left = nullptr before returning.
};

class MCMod : public MCMultiBinaryOperatorCtxt<
        MCMathEvalMod,
        MCMathEvalModArrayByNumber,
        MCMathEvalModArrayByArray,
        EE_MOD_BADLEFT,
        EE_MOD_BADRIGHT,
        EE_MOD_MISMATCH,
        false,
        FR_MULDIV>
{ public: HXTExprType hxt_expr_type() const override { return kHXTExpr_Mod; } };

class MCWrap : public MCMultiBinaryOperatorCtxt<
        MCMathEvalWrap,
        MCMathEvalWrapArrayByNumber,
        MCMathEvalWrapArrayByArray,
        EE_WRAP_BADLEFT,
        EE_WRAP_BADRIGHT,
        EE_WRAP_MISMATCH,
        false,
        FR_MULDIV>
{};

class MCNot : public MCUnaryOperator
{
public:
    MCNot()
    {
        rank = FR_UNARY;
    }

    virtual void eval_ctxt(MCExecContext &ctxt, MCExecValue &r_value);
    HXTExprType hxt_expr_type() const override { return kHXTExpr_Not; }
};

class MCNotBits : public MCUnaryOperatorCtxt<uinteger_t, MCMathEvalBitwiseNot, EE_NOTBITS_BADRIGHT, FR_UNARY>
{};

class MCNotEqual : public MCBinaryOperatorCtxt<MCValueRef, bool, MCLogicEvalIsNotEqualTo, EE_FACTOR_BADLEFT, EE_FACTOR_BADRIGHT, FR_EQUAL>
{ public: HXTExprType hxt_expr_type() const override { return kHXTExpr_NotEqual; } };

class MCOr : public MCExpression
{
public:
	MCOr()
	{
		rank = FR_OR;
    }
    virtual void eval_ctxt(MCExecContext &ctxt, MCExecValue &r_value);
    virtual bool hxt_serialize(MCHXTASTWriter &w) const override;
    virtual bool hxt_deserialize_body(MCHXTASTReader &r) override;
};

class MCOrBits : public MCBinaryOperatorCtxt<uinteger_t, uinteger_t, MCMathEvalBitwiseOr, EE_ORBITS_BADLEFT, EE_ORBITS_BADRIGHT, FR_OR_BITS>
{};

class MCOver : public MCMultiBinaryOperatorCtxt<
        MCMathEvalOver,
        MCMathEvalOverArrayByNumber,
        MCMathEvalOverArrayByArray,
        EE_OVER_BADLEFT,
        EE_OVER_BADRIGHT,
        EE_OVER_MISMATCH,
        false,
        FR_MULDIV>
{ public: HXTExprType hxt_expr_type() const override { return kHXTExpr_Div; } };

class MCPlus : public MCMultiBinaryCommutativeOperatorCtxt<
        MCMathEvalAdd,
        MCMathEvalAddNumberToArray,
        MCMathEvalAddArrayToArray,
        EE_PLUS_BADLEFT,
        EE_PLUS_BADRIGHT,
        true,
        FR_ADDSUB>
{ public: HXTExprType hxt_expr_type() const override { return kHXTExpr_Add; } };

class MCPow : public MCBinaryOperatorCtxt<double, double, MCMathEvalPower, EE_POW_BADLEFT, EE_POW_BADRIGHT, FR_POW>
{ public: HXTExprType hxt_expr_type() const override { return kHXTExpr_Power; } };

class MCThere : public MCExpression
{
	Is_type form;
	MCChunk *object;
	There_mode mode;
public:
	MCThere()
	{
		rank = FR_UNARY;
		form = IT_NORMAL;
		object = NULL;
		mode = TM_UNDEFINED;
	}
	virtual ~MCThere();
	virtual Parse_stat parse(MCScriptPoint &, Boolean the);
    virtual void eval_ctxt(MCExecContext &ctxt, MCExecValue &r_value);
    // HXT: stores form(u8) + mode(u8) + <chunk-expr>.
    virtual bool hxt_serialize(MCHXTASTWriter &w) const override;
    virtual bool hxt_deserialize_body(MCHXTASTReader &r) override;
};

class MCTimes : public MCMultiBinaryCommutativeOperatorCtxt<
        MCMathEvalMultiply,
        MCMathEvalMultiplyArrayByNumber,
        MCMathEvalMultiplyArrayByArray,
        EE_TIMES_BADLEFT,
        EE_TIMES_BADRIGHT,
        false,
        FR_MULDIV>
{
public:
    HXTExprType hxt_expr_type() const override { return kHXTExpr_Mul; }
};

class MCXorBits : public MCBinaryOperatorCtxt<uinteger_t, uinteger_t, MCMathEvalBitwiseXor, EE_XORBITS_BADLEFT, EE_XORBITS_BADRIGHT, FR_XOR_BITS>
{};

class MCBeginsEndsWith : public MCBinaryOperator
{
public:
    MCBeginsEndsWith(void)
    {
        rank = FR_COMPARISON;
    }
    virtual Parse_stat parse(MCScriptPoint&, Boolean the);
};

class MCBeginsWith : public MCBeginsEndsWith
{
public:
    virtual void eval_ctxt(MCExecContext &ctxt, MCExecValue &r_value);
};

class MCEndsWith : public MCBeginsEndsWith
{
public:
    virtual void eval_ctxt(MCExecContext &ctxt, MCExecValue &r_value);
};

class MCMatches : public MCBinaryOperator
{
public:
    virtual Parse_stat parse(MCScriptPoint&, Boolean the);
    virtual void eval_ctxt(MCExecContext &ctxt, MCExecValue &r_value);
private:
    Match_mode m_matchmode;
};

#endif

/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/*!
 * \file  operator_common.h
 * \brief common internal header of most operators
 *   this header includes utility functions operator can use
 * \author Bing Xu
*/
#ifndef MXNET_OPERATOR_OPERATOR_COMMON_H_
#define MXNET_OPERATOR_OPERATOR_COMMON_H_

#include <dmlc/json.h>
#include <dmlc/logging.h>
#include <mxnet/operator.h>
#include <mxnet/ndarray.h>
#include <mxnet/op_attr_types.h>
#include <mxnet/base.h>
#include <istream>
#include <ostream>
#include <string>
#include <vector>
#include "../common/cuda_utils.h"
#include "../common/utils.h"

namespace mxnet {
namespace op {
/*!
 * \brief assign the expression to out according to request
 * \param out the data to be assigned
 * \param req the assignment request
 * \param exp the expression
 * \tparam OType output type
 * \tparam Exp expression type
 */
#define Assign(out, req, exp)           \
  {                                     \
    switch (req) {                      \
      case kNullOp:                     \
        break;                          \
      case kWriteTo:                    \
      case kWriteInplace:               \
        (out) = (exp);                  \
        break;                          \
      case kAddTo:                      \
        (out) += (exp);                 \
        break;                          \
      default:                          \
        LOG(FATAL) << "not reached";    \
    }                                   \
  }


/*! \brief exception throwed by InferShape error */
struct InferShapeError : public dmlc::Error {
  /*! \brief analyze message */
  std::string msg;
  /*! \brief corresponding input index */
  int index;
  // constructor
  InferShapeError(const std::string& msg_, int index)
    : dmlc::Error(msg_), msg(msg_), index(index) {}
};

/*! \brief exception throwed by InferShape error */
struct InferTypeError : public dmlc::Error {
  /*! \brief analyze message */
  std::string msg;
  /*! \brief corresponding input index */
  int index;
  // constructor
  InferTypeError(const std::string& msg_, int index)
    : dmlc::Error(msg_), msg(msg_), index(index) {}
};

/*! \brief check if shape is empty or contains unkown (0) dim. */
inline bool shape_is_none(const TShape& x) {
  return x.ndim() == 0 || x.Size() == 0;
}

/*! \brief check if type is none (-1) */
inline bool type_is_none(const int& x) {
  return x == -1;
}

/*! \brief check if shape is scalar({1}). */
inline bool shape_is_scalar(const TShape& x) {
  return x.ndim() == 1 && x.Size() == 1;
}

/*! \brief get string representation of shape */
inline std::string shape_string(const TShape& x) {
  std::ostringstream os;
  os << x;
  return os.str();
}

/*! \brief get string representation of shape */
inline std::string type_string(const int& x) {
  switch (x) {
    case mshadow::kFloat32:
      return "float32";
    case mshadow::kFloat64:
      return "float64";
    case mshadow::kFloat16:
      return "float16";
    case mshadow::kUint8:
      return "uint8";
    case mshadow::kInt32:
      return "int32";
  }
  return "unknown";
}

/*! \brief get string representation of storage_type */
inline std::string stype_string(const int& x) {
  switch (x) {
    case kDefaultStorage:
      return "default";
    case kCSRStorage:
      return "csr";
    case kRowSparseStorage:
      return "row_sparse";
  }
  return "unknown";
}

/*!
 * \brief Assign x to y. Checks for compatiblity when y is not empty.
 *  Allow missing dim in both x and y (as 0).
 * \param y target shape.
 * \param x source shape.
 * \return whether x and y are compatible.
 */
inline bool shape_assign(TShape *y, const TShape& x) {
  if (y->ndim() == 0) {
    *y = x;
    return true;
  } else if (y->ndim() != x.ndim()) {
    return x.ndim() == 0;
  } else {
    for (size_t i = 0; i < y->ndim(); ++i) {
      if ((*y)[i] == 0) {
        (*y)[i] = x[i];
      } else if ((*y)[i] != x[i] && x[i] != 0) {
        return false;
      }
    }
    return true;
  }
}

/*!
 * \brief Assign x to y. Checks for compatiblity when y is not -1.
 * \param y target type.
 * \param x source type.
 * \return whether x and y are compatible.
 */
inline bool type_assign(int *y, const int& x) {
  if (*y == -1) {
    *y = x;
    return true;
  } else if (*y != x && x != -1) {
    return false;
  }
  return true;
}

/*!
 * \brief macro assign shape to out if out is unknown otherwise check consistency
 *  Use macro so we can see the error file more clearly
 * \param shape_array the shape array to store the result
 * \param index the index of in the array
 * \param shape the inferred shape
 */
#define SHAPE_ASSIGN_CHECK(shape_array, index, shape)                       \
  {                                                                         \
    if (!shape_assign(&(shape_array)[index], TShape(shape))) {              \
      std::ostringstream os;                                                \
      os << "Shape inconsistent, Provided=" << (shape_array)[index] << ','  \
         << " inferred shape=" << shape;                                    \
      throw ::mxnet::op::InferShapeError(os.str(), index);                  \
    }                                                                       \
  }

/*!
 * \brief macro assign type to out if out is unknown (-1) otherwise check consistency
 *  Use macro so we can see the error file more clearly
 * \param type_array the type array to store the result
 * \param index the index of in the array
 * \param type the inferred type
 */
#define TYPE_ASSIGN_CHECK(type_array, index, type)                          \
  {                                                                         \
    if (!type_assign(&(type_array)[index], type)) {                         \
      std::ostringstream os;                                                \
      os << "Type inconsistent, Provided="                                  \
         << type_string((type_array)[index]) << ','                         \
         << " inferred type=" << type_string(type);                         \
      throw ::mxnet::op::InferTypeError(os.str(), index);                   \
    }                                                                       \
  }

/*!
 * \brief macro check if type is the same as expected.
 * \param type the type to be checked
 * \param expected the expected type
 */
#define UNIFORM_TYPE_CHECK(type, expected, arg)                         \
  {                                                                     \
    CHECK_EQ(type, expected) << "This layer requires uniform type. "    \
                             << "Expected '" << type_string(expected)   \
                             << "' v.s. given '" << type_string(type)   \
                             << "' at '" << arg << "'";                 \
  }

/*!
 * \brief macro assign type to out if out is unknown (-1) otherwise check consistency
 *  Use macro so we can see the error file more clearly
 * \param type_array the storage type array to store the result
 * \param index the index of in the array
 * \param type the inferred storage type
 */
#define STORAGE_TYPE_ASSIGN_CHECK(type_array, index, type)                  \
  {                                                                         \
    if (!type_assign(&(type_array)[index], type)) {                         \
      std::ostringstream os;                                                \
      os << "Storage type inconsistent, Provided="                          \
         << stype_string((type_array)[index]) << ','                        \
         << " inferred storage type=" << stype_string(type);                \
      throw ::mxnet::op::InferTypeError(os.str(), index);                   \
    }                                                                       \
  }

// helper macro to implement bind dispatch
#if MXNET_USE_CUDA
#define DO_BIND_DISPATCH(Method, ...)                                \
  if (ctx.dev_mask() == cpu::kDevMask) {                             \
      return Method<cpu>(__VA_ARGS__);                               \
    } else {                                                         \
      return Method<gpu>(__VA_ARGS__);                               \
    }
#else
#define DO_BIND_DISPATCH(Method, ...)                                \
  if (ctx.dev_mask() == cpu::kDevMask) {                             \
    return Method<cpu>(__VA_ARGS__);                                 \
  } else {                                                           \
    LOG(FATAL) << "GPU is not enabled";                              \
    return nullptr;                                                  \
  }
#endif


// make a new node with operator op_name. Inputs are not filled.
inline nnvm::NodePtr MakeNode(
    const char* op_name, const std::string& name,
    std::vector<nnvm::NodeEntry> const * inputs,
    std::unordered_map<std::string, std::string> const * dict,
    nnvm::NodePtr const * fwd_node) {
  auto p = nnvm::Node::Create();
  p->attrs.op = nnvm::Op::Get(op_name);
  p->attrs.name = name;
  if (dict != nullptr) p->attrs.dict = *dict;
  if (inputs != nullptr) p->inputs = *inputs;
  if (fwd_node != nullptr) {
    p->control_deps.emplace_back(*fwd_node);
  }
  if (p->op()->attr_parser != nullptr) {
    p->op()->attr_parser(&(p->attrs));
  }
  return p;
}

inline nnvm::NodePtr MakeNode(
    const char* op_name, const std::string& name,
    const std::vector<nnvm::NodeEntry>& inputs,
    std::unordered_map<std::string, std::string> const * dict,
    nnvm::NodePtr const * fwd_node) {
  return MakeNode(op_name, name, &inputs, dict, fwd_node);
}


// quick helper to make node
inline std::vector<nnvm::NodeEntry> MakeGradNode(
    const char* op_name, const nnvm::NodePtr& n,
    const std::vector<nnvm::NodeEntry>& inputs,
    const std::unordered_map<std::string, std::string>& dict) {
  auto p = MakeNode(op_name, n->attrs.name + "_backward",
                    &inputs, &dict, &n);
  std::vector<nnvm::NodeEntry> ret;
  for (index_t i = 0; i < p->num_outputs(); ++i) {
    ret.emplace_back(nnvm::NodeEntry{p, i, 0});
  }
  return ret;
}

// quick helper to make gradient nodes that simply pass back zero. could be used in output ops.
inline std::vector<nnvm::NodeEntry> MakeZeroGradNodes(
    const nnvm::NodePtr& n,
    const std::vector<nnvm::NodeEntry>& ograds) {
  std::vector<nnvm::NodeEntry> ret;
  for (index_t i = 0; i < n->num_inputs(); ++i) {
    std::ostringstream os;
    if (1 == n->num_inputs()) {
      os << n->attrs.name << "_backward";
    } else {
      os << n->attrs.name << "_in" << i << "_backward";
    }
    auto p = MakeNode("zeros_like", os.str(), {n->inputs[i]}, nullptr, &n);
    ret.emplace_back(nnvm::NodeEntry{p, 0, 0});
  }
  return ret;
}


// check whether all output grads are zero.
inline bool CheckGradAllZero(const std::vector<nnvm::NodeEntry>& ograds) {
  const auto zero_op = nnvm::Op::Get("_zeros");
  const auto zero_like_op = nnvm::Op::Get("zeros_like");
  if (!ograds.size()) return false;
  for (const auto& grad : ograds) {
    if (!grad.node) return false;
    if (grad.node->op() != zero_op && grad.node->op() != zero_like_op ) return false;
  }
  return true;
}

// make gradient node that doesn't add to objective.
// i.e. igrads are always zero when ograds are zero.
inline std::vector<nnvm::NodeEntry> MakeNonlossGradNode(
    const char* op_name, const nnvm::NodePtr& n,
    const std::vector<nnvm::NodeEntry>& ograds,
    const std::vector<nnvm::NodeEntry>& inputs,
    const std::unordered_map<std::string, std::string> dict) {
  if (CheckGradAllZero(ograds)) return MakeZeroGradNodes(n, ograds);
  auto p = MakeNode(op_name, n->attrs.name + "_backward",
                    nullptr, &dict, &n);
  p->inputs.insert(p->inputs.end(), ograds.begin(), ograds.end());
  p->inputs.insert(p->inputs.end(), inputs.begin(), inputs.end());
  std::vector<nnvm::NodeEntry> ret;
  for (index_t i = 0; i < p->num_outputs(); ++i) {
    ret.emplace_back(nnvm::NodeEntry{p, i, 0});
  }
  return ret;
}

/*! \brief Parse keyword arguments as PType arguments and save to parsed */
template<typename PType>
inline void ParamParser(nnvm::NodeAttrs* attrs) {
  PType param;
  try {
    param.Init(attrs->dict);
  } catch (const dmlc::ParamError& e) {
    std::ostringstream os;
    os << e.what();
    os << ", in operator " << attrs->op->name << "("
       << "name=\"" << attrs->name << "\"";
    for (const auto& k : attrs->dict) {
      os << ", " << k.first << "=\"" << k.second << "\"";
    }
    os << ")";
    throw dmlc::ParamError(os.str());
  }
  attrs->parsed = std::move(param);
}

/*! \brief Perform storage fallback to invoke fcompute.
 *  \param attrs attributes of the operator
 *  \param ctx operator context
 *  \param inputs inputs of fcompute
 *  \param req req of fcompute
 *  \param outputs outputs of fcompute
 *  \param fcompute
 *  \param fname name of the operator
 *  \param mutate_idx the indices of mutable inputs
 */
template <typename xpu>
void FCompExFallback(const nnvm::NodeAttrs& attrs,
                     const OpContext& ctx,
                     const std::vector<NDArray>& inputs,
                     const std::vector<OpReqType>& req,
                     const std::vector<NDArray>& outputs,
                     FCompute fcompute,
                     const std::string& fname,
                     std::vector<uint32_t> mutate_idx = {}) {
  using namespace mxnet::common;
  std::vector<TBlob> in_blobs, out_blobs;
  std::vector<NDArray> pre_temp_src, pre_temp_dst, post_temp_dst, post_temp_src;
  // mapping from index in input_blobs to index in pre_temp_dst
  std::unordered_map<uint32_t, uint32_t> in_temp_idx_map;
  SetupDefaultBlobs(inputs, &in_blobs, &pre_temp_src, &pre_temp_dst, &in_temp_idx_map);
  SetupDefaultBlobs(outputs, &out_blobs, &post_temp_dst, &post_temp_src);
  for (const auto idx : mutate_idx) {
    auto map_iter = in_temp_idx_map.find(idx);
    if (map_iter != in_temp_idx_map.end()) {
      post_temp_src.push_back(pre_temp_dst[map_iter->second]);
      post_temp_dst.push_back(inputs[idx]);
    }
  }
  CastNonDefaultStorage<xpu>(pre_temp_src, pre_temp_dst, ctx, true);
  fcompute(attrs, ctx, in_blobs, req, out_blobs);
  CastNonDefaultStorage<xpu>(post_temp_src, post_temp_dst, ctx, true);
}

#define CHECK_RSP_ALL_ROWS_NON_ZERO(rsp, func, param)                              \
  {                                                                                \
    CHECK(rsp.storage_shape()[0] == rsp.shape()[0]) << func                        \
          << " for RowSparse " << param << " is only implemented for "             \
          << "RowSparse " << param << " with all rows containing non-zeros. "      \
          << "Expects " << param << ".values.shape[0] (" << rsp.storage_shape()[0] \
          << ") == " << param << ".shape[0] (" << rsp.shape()[0] << ").";          \
  }


}  // namespace op
}  // namespace mxnet
#endif  // MXNET_OPERATOR_OPERATOR_COMMON_H_

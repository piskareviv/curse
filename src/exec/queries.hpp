#pragma once

#include <chrono>
#include <cstdlib>
#include <format>
#include <iostream>
#include <memory>
#include <string>

#include "src/core/convert.hpp"
#include "src/core/csv.hpp"
#include "src/core/operators.hpp"
#include "src/core/operators/drop.hpp"
#include "src/core/operators/filter.hpp"
#include "src/core/operators/select.hpp"
#include "src/core/operators/transform.hpp"
#include "src/core/storage.hpp"
#include "src/core/types.hpp"
#include "src/exec/hits_schema.hpp"
#include "src/util/assert.hpp"

namespace Q {           // NOLINT
using namespace curse;  // NOLINT

curse::Schema SubSchema(const curse::Schema& schema, std::vector<std::string> sub_schema);

std::unique_ptr<BatchStream> Q0(const std::string& file);
std::unique_ptr<BatchStream> Q1(const std::string& file);
std::unique_ptr<BatchStream> Q2(const std::string& file);
std::unique_ptr<BatchStream> Q3(const std::string& file);
std::unique_ptr<BatchStream> Q4(const std::string& file);
std::unique_ptr<BatchStream> Q5(const std::string& file);
std::unique_ptr<BatchStream> Q6(const std::string& file);
std::unique_ptr<BatchStream> Q7(const std::string& file);
std::unique_ptr<BatchStream> Q8(const std::string& file);
std::unique_ptr<BatchStream> Q9(const std::string& file);
std::unique_ptr<BatchStream> Q10(const std::string& file);
std::unique_ptr<BatchStream> Q11(const std::string& file);
std::unique_ptr<BatchStream> Q12(const std::string& file);
std::unique_ptr<BatchStream> Q13(const std::string& file);
std::unique_ptr<BatchStream> Q14(const std::string& file);
std::unique_ptr<BatchStream> Q15(const std::string& file);
std::unique_ptr<BatchStream> Q16(const std::string& file);
std::unique_ptr<BatchStream> Q17(const std::string& file);
std::unique_ptr<BatchStream> Q18(const std::string& file);
std::unique_ptr<BatchStream> Q19(const std::string& file);
std::unique_ptr<BatchStream> Q20(const std::string& file);
std::unique_ptr<BatchStream> Q21(const std::string& file);
std::unique_ptr<BatchStream> Q22(const std::string& file);
std::unique_ptr<BatchStream> Q23(const std::string& file);
std::unique_ptr<BatchStream> Q24(const std::string& file);
std::unique_ptr<BatchStream> Q25(const std::string& file);
std::unique_ptr<BatchStream> Q26(const std::string& file);
std::unique_ptr<BatchStream> Q27(const std::string& file);
std::unique_ptr<BatchStream> Q28(const std::string& file);
std::unique_ptr<BatchStream> Q29(const std::string& file);
std::unique_ptr<BatchStream> Q30(const std::string& file);
std::unique_ptr<BatchStream> Q31(const std::string& file);
std::unique_ptr<BatchStream> Q32(const std::string& file);
std::unique_ptr<BatchStream> Q33(const std::string& file);
std::unique_ptr<BatchStream> Q34(const std::string& file);
std::unique_ptr<BatchStream> Q35(const std::string& file);
std::unique_ptr<BatchStream> Q36(const std::string& file);
std::unique_ptr<BatchStream> Q37(const std::string& file);
std::unique_ptr<BatchStream> Q38(const std::string& file);
std::unique_ptr<BatchStream> Q39(const std::string& file);
std::unique_ptr<BatchStream> Q40(const std::string& file);
std::unique_ptr<BatchStream> Q41(const std::string& file);
std::unique_ptr<BatchStream> Q42(const std::string& file);

}  // namespace Q

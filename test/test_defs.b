type basic_01 {a int, c string}

var empty_r1 basic_01;

var extend_1 basic_01;

var one_r1 basic_01;

var one_r1_cpy basic_01;

var one_r2 basic_01;

var perf_rel basic_01;

var two_r2 basic_01;

var tx_empty basic_01;

var tx_target1 basic_01;

var tx_target2 basic_01;

var tx_target3 basic_01;

var compound_1 basic_01;

var compound_1_res basic_01;

var param_1 basic_01;

type basic_02 {id int, string_val string, int_val int, real_val real}

var join_1_r1 basic_02;

var storage_r1 basic_02;

var storage_r2 basic_02;

var union_1_l basic_02;

var union_1_r basic_02;

var union_1_res basic_02;

type basic_03 {a int, b real, c string}

var join_2_r1 basic_03;

var join_2_res basic_03;

var project_1 basic_03;

var project_2 basic_03;

var rename_1 basic_03;

var select_1 basic_03;

var select_1_res basic_03;

var select_2 basic_03;

var select_2_res basic_03;

var select_3 basic_03;

var select_3_res basic_03;

var semidiff_1_l basic_03;

var semidiff_1_res basic_03;

var semidiff_2_l basic_03;

var semidiff_2_res basic_03;

type join_test1_r2 {r2_id int, r2_string_val string, id int, r2_real_val real}

var join_1_r2 join_test1_r2;

type join_test1_res {id int, string_val string, r2_id int, r2_string_val string, int_val int, real_val real, r2_real_val real}

var join_1_res join_test1_res;

type join_test2_r2 {a int, b real}

var join_2_r2 join_test2_r2;

type prj_res {b real, c string}

var project_1_res prj_res;

var project_2_res prj_res;

type empty_test_2 {name string, something real}

var empty_r2 empty_test_2;

type equal_test {name string, something real, x string}

var equal_r1 equal_test;

var equal_r2 equal_test;

var equal_r3 equal_test;

type basic_06 {a int, c string, d int}

var extend_res_1 basic_06;

var extend_res_2 basic_06;

type io_test {id int}

var io io_test;

var perf_io io_test;

type rename_test1_res {new_id int, new_name string, new_some real}

var rename_1_res rename_test1_res;

type semidiff_r {a int, b real, d real}

var semidiff_1_r semidiff_r;

var semidiff_2_r semidiff_r;

type summary_test_dep {dep_name string}

var summary_dep summary_test_dep;

type summary_test_emp {age int, name string, salary real, birth long, dep_name string}

var summary_emp summary_test_emp;

type summary_test_1 {dep_name string, employees int, min_age int, max_age int, avg_age real, add_age int, min_salary real, max_salary real, avg_salary real, add_salary real, min_birth long, max_birth long, avg_birth real, add_birth long}

var summary_res_1 summary_test_1;

type summary_test_2 {min_age int, max_age int, min_salary real, max_salary real, count int, add_salary real}

var summary_res_2 summary_test_2;

type Bookings {date string, when int, pos int, car string, name string, email string, phone string}

var union_2_l Bookings;

var union_2_r Bookings;

var union_2_res Bookings;

var inline_var {x y real};

fn load_compound_1() basic_01
{
	return compound_1;
}

fn load_compound_1_res() basic_01
{
	return compound_1_res;
}

fn load_empty_r1() basic_01
{
	return empty_r1;
}

fn load_empty_r2() empty_test_2
{
	return empty_r2;
}

fn load_equal_r1() equal_test
{
	return equal_r1;
}

fn load_equal_r2() equal_test
{
	return equal_r2;
}

fn load_equal_r3() equal_test
{
	return equal_r3;
}

fn load_extend_1() basic_01
{
	return extend_1;
}

fn load_extend_res_1() basic_06
{
	return extend_res_1;
}

fn load_extend_res_2() basic_06
{
	return extend_res_2;
}

fn load_io() io_test
{
	return io;
}

fn load_join_1_r1() basic_02
{
	return join_1_r1;
}

fn load_join_1_r2() join_test1_r2
{
	return join_1_r2;
}

fn load_join_1_res() join_test1_res
{
	return join_1_res;
}

fn load_join_2_r1() basic_03
{
	return join_2_r1;
}

fn load_join_2_r2() join_test2_r2
{
	return join_2_r2;
}

fn load_join_2_res() basic_03
{
	return join_2_res;
}

fn load_one_r1() basic_01
{
	return one_r1;
}

fn load_one_r2() basic_01
{
	return one_r2;
}

fn load_one_r1_cpy() basic_01
{
	return one_r1_cpy;
}

fn load_param_1() basic_01
{
	return param_1;
}

fn load_perf_io() io_test
{
	return perf_io;
}

fn load_perf_rel() basic_01
{
	return perf_rel;
}

fn load_project_1() basic_03
{
	return project_1;
}

fn load_project_1_res() prj_res
{
	return project_1_res;
}

fn load_project_2() basic_03
{
	return project_2;
}

fn load_project_2_res() prj_res
{
	return project_2_res;
}

fn load_rename_1() basic_03
{
	return rename_1;
}

fn load_rename_1_res() rename_test1_res
{
	return rename_1_res;
}

fn load_select_1() basic_03
{
	return select_1;
}

fn load_select_1_res() basic_03
{
	return select_1_res;
}

fn load_select_2() basic_03
{
	return select_2;
}

fn load_select_2_res() basic_03
{
	return select_2_res;
}

fn load_select_3() basic_03
{
	return select_3;
}

fn load_select_3_res() basic_03
{
	return select_3_res;
}

fn load_semidiff_1_l() basic_03
{
	return semidiff_1_l;
}

fn load_semidiff_1_r() semidiff_r
{
	return semidiff_1_r;
}

fn load_semidiff_1_res() basic_03
{
	return semidiff_1_res;
}

fn load_semidiff_2_l() basic_03
{
	return semidiff_2_l;
}

fn load_semidiff_2_r() semidiff_r
{
	return semidiff_2_r;
}

fn load_semidiff_2_res() basic_03
{
	return semidiff_2_res;
}

fn load_storage_r1() basic_02
{
	return storage_r1;
}

fn load_storage_r2() basic_02
{
	return storage_r2;
}

fn load_summary_dep() summary_test_dep
{
	return summary_dep;
}

fn load_summary_emp() summary_test_emp
{
	return summary_emp;
}

fn load_summary_res_1() summary_test_1
{
	return summary_res_1;
}

fn load_summary_res_2() summary_test_2
{
	return summary_res_2;
}

fn load_two_r2() basic_01
{
	return two_r2;
}

fn load_tx_empty() basic_01
{
	return tx_empty;
}

fn load_tx_target1() basic_01
{
	return tx_target1;
}

fn load_tx_target2() basic_01
{
	return tx_target2;
}

fn load_tx_target3() basic_01
{
	return tx_target3;
}

fn load_union_1_l() basic_02
{
	return union_1_l;
}

fn load_union_1_r() basic_02
{
	return union_1_r;
}

fn load_union_1_res() basic_02
{
	return union_1_res;
}

fn load_union_2_l() Bookings
{
	return union_2_l;
}

fn load_union_2_r() Bookings
{
	return union_2_r;
}

fn load_union_2_res() Bookings
{
	return union_2_res;
}

fn store_compound_1(x basic_01) void
{
	compound_1 = x;
}

fn store_compound_1_res(x basic_01) void
{
	compound_1_res = x;
}

fn store_empty_r1(x basic_01) void
{
	empty_r1 = x;
}

fn store_empty_r2(x empty_test_2) void
{
	empty_r2 = x;
}

fn store_equal_r1(x equal_test) void
{
	equal_r1 = x;
}

fn store_equal_r2(x equal_test) void
{
	equal_r2 = x;
}

fn store_equal_r3(x equal_test) void
{
	equal_r3 = x;
}

fn store_extend_1(x basic_01) void
{
	extend_1 = x;
}

fn store_extend_res_1(x basic_06) void
{
	extend_res_1 = x;
}

fn store_extend_res_2(x basic_06) void
{
	extend_res_2 = x;
}

fn store_io(x io_test) void
{
	io = x;
}

fn store_join_1_r1(x basic_02) void
{
	join_1_r1 = x;
}

fn store_join_1_r2(x join_test1_r2) void
{
	join_1_r2 = x;
}

fn store_join_1_res(x join_test1_res) void
{
	join_1_res = x;
}

fn store_join_2_r1(x basic_03) void
{
	join_2_r1 = x;
}

fn store_join_2_r2(x join_test2_r2) void
{
	join_2_r2 = x;
}

fn store_join_2_res(x basic_03) void
{
	join_2_res = x;
}

fn store_one_r1(x basic_01) void
{
	one_r1 = x;
}

fn store_one_r1_cpy(x basic_01) void
{
	one_r1_cpy = x;
}

fn store_one_r2(x basic_01) void
{
	one_r2 = x;
}

fn store_param_1(x basic_01) void
{
	param_1 = x;
}

fn store_perf_io(x io_test) void
{
	perf_io = x;
}

fn store_perf_rel(x basic_01) void
{
	perf_rel = x;
}

fn store_project_1(x basic_03) void
{
	project_1 = x;
}

fn store_project_1_res(x prj_res) void
{
	project_1_res = x;
}

fn store_project_2(x basic_03) void
{
	project_2 = x;
}

fn store_project_2_res(x prj_res) void
{
	project_2_res = x;
}

fn store_rename_1(x basic_03) void
{
	rename_1 = x;
}

fn store_rename_1_res(x rename_test1_res) void
{
	rename_1_res = x;
}

fn store_select_1(x basic_03) void
{
	select_1 = x;
}

fn store_select_1_res(x basic_03) void
{
	select_1_res = x;
}

fn store_select_2(x basic_03) void
{
	select_2 = x;
}

fn store_select_2_res(x basic_03) void
{
	select_2_res = x;
}

fn store_select_3(x basic_03) void
{
	select_3 = x;
}

fn store_select_3_res(x basic_03) void
{
	select_3_res = x;
}

fn store_semidiff_1_l(x basic_03) void
{
	semidiff_1_l = x;
}

fn store_semidiff_1_r(x semidiff_r) void
{
	semidiff_1_r = x;
}

fn store_semidiff_1_res(x basic_03) void
{
	semidiff_1_res = x;
}

fn store_semidiff_2_l(x basic_03) void
{
	semidiff_2_l = x;
}

fn store_semidiff_2_r(x semidiff_r) void
{
	semidiff_2_r = x;
}

fn store_semidiff_2_res(x basic_03) void
{
	semidiff_2_res = x;
}

fn store_storage_r1(x basic_02) void
{
	storage_r1 = x;
}

fn store_storage_r2(x basic_02) void
{
	storage_r2 = x;
}

fn store_summary_dep(x summary_test_dep) void
{
	summary_dep = x;
}

fn store_summary_emp(x summary_test_emp) void
{
	summary_emp = x;
}

fn store_summary_res_1(x summary_test_1) void
{
	summary_res_1 = x;
}

fn store_summary_res_2(x summary_test_2) void
{
	summary_res_2 = x;
}

fn store_two_r2(x basic_01) void
{
	two_r2 = x;
}

fn store_tx_empty(x basic_01) void
{
	tx_empty = x;
}

fn store_tx_target1(x basic_01) void
{
	tx_target1 = x;
}

fn store_tx_target2(x basic_01) void
{
	tx_target2 = x;
}

fn store_tx_target3(x basic_01) void
{
	tx_target3 = x;
}

fn store_union_1_l(x basic_02) void
{
	union_1_l = x;
}

fn store_union_1_r(x basic_02) void
{
	union_1_r = x;
}

fn store_union_1_res(x basic_02) void
{
	union_1_res = x;
}

fn store_union_2_l(x Bookings) void
{
	union_2_l = x;
}

fn store_union_2_r(x Bookings) void
{
	union_2_r = x;
}

fn store_union_2_res(x Bookings) void
{
	union_2_res = x;
}

type Signature {
    fname string,
    pname string,
    pattr string,
    ptype string
}

var sign Signature;
var sign1 Signature;

fn load_sign() Signature
{
    return sign;
}

fn store_sign(x Signature) void
{
    sign = x;
}

fn load_sign1() Signature
{
    return sign1;
}

fn store_sign1(x Signature) void
{
    sign1 = x;
}

fn test_sign1(i int, rr Bookings, s string, r real, l long) Signature
{
    return sign;
}

fn test_sign2() void
{
}

fn test_sign3() Signature
{
    return sign;
}

fn test_sign4(i int) void
{
}

fn test_sign5(rr Bookings) void
{
}

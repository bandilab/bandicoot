type test {i int, r real, s string}

var t test;

type test_res {i int, r real, s string, ix int, rx real}

fn s_not_int() test
{
	return (select !i t);
}

fn s_eq_int() test
{
	return (select i == 1 t);
}

fn s_eq_real() test
{
	return (select r == .3 t);
}

fn s_eq_string() test
{
	return (select s == "abc" t);
}

fn s_neq_int() test
{
	return (select i != 1 t);
}

fn s_neq_real() test
{
	return (select r != .3 t);
}

fn s_neq_string() test
{
	return (select s != "abc" t);
}

fn s_and_basic() test
{
	return (select i && 1 t);
}

fn s_and_complex() test
{
	return (select i == 1 && r != .3 && s == "abc" t);
}

fn s_or_basic() test
{
	return (select i || 0 t);
}

fn s_or_complex() test
{
	return (select i == 1 || r != .3 || s == "abc" t);
}

fn s_gt() test
{
	return (select i > 0 && r > 0.0 && s > "" t);
}

fn s_lt() test
{
	return (select i < 0 && r < 0.0 && s < "abc" t);
}

fn s_sum() test_res
{
	return (extend ix = i + 1, rx = r + 0.3 t);
}

fn s_sub() test_res
{
	return (extend ix = 1 - i, rx = 0.3 - r t);
}

fn s_mul() test_res
{
	return (extend ix = 2 * i, rx = 0.1 * r t);
}

fn s_div() test_res
{
	return (extend ix = 100 / i, rx = 100.0 / r t);
}

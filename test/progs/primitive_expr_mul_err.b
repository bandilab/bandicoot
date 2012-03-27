type test {i int, s string}

var t test;

type test_res {i int, s string, x int}

fn s_mul_err() test_res
{
	return (extend x = i * s t);
}

type test {i int, r real}

var t test;

type test_res {i int, r real, x int}

fn s_sum_err() void
{
	return (extend x = i + r t);
}

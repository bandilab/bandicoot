type test {i int, r real}

var t test;

fn s_neq_err() test
{
	return (select i != r t);
}

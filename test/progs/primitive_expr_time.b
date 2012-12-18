type T {l long}

fn test(t {i int}) T
{
    var x = select ((long i) == Time.Now + 10L) t;
    return extend (l = Time.Now) t;
}

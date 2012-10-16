type Books {
    title string,
    price real,
}

var shelf Books;
var priceSeq {price real};

fn Reset() void {
    shelf -= shelf;
    priceSeq -= priceSeq;
}

fn Return() Books {
    return shelf;
}

fn Echo(b Books) Books {
    return b;
}

fn TmpReturn(b Books) Books {
    var tmp = b + shelf;
    return tmp;
}

fn ReRead(b Books) void {
    shelf = shelf;
}

fn StoreReturn(b Books) Books {
    shelf += b;
    return shelf;
}

fn IndirectReturn() Books {
    return Return;
}

fn IndirectReturnStore() Books {
    var x = Return;
    shelf = shelf;
    return x;
}

fn IndirectStoreReturn(b Books) Books {
    return StoreReturn b;
}

fn IndirectEcho(b Books) Books {
    return Echo b;
}

fn IndirectTmpReturn(b Books) Books {
    return TmpReturn b;
}

fn IndirectReRead(b Books) void {
    ReRead b;
}

fn NextPrice() {price real} {
    priceSeq = project (price)
        (extend price = m + 1.0
             (summary m = (max price 0.0) priceSeq));

    return priceSeq;
}

fn IndirectNextPrice() Books {
    return extend title = "hello1" NextPrice;
}

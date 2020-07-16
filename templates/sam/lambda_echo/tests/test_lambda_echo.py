from lambda_echo import lambda_handler

def test_should_return_empty():
    assert {} == lambda_handler(event = {}, context = {})

def test_should_return_same_object():
    assert { 'x': 1 } == lambda_handler(event = { 'x': 1 }, context = {})

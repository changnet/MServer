use test_999
db.createUser( {user:"test",pwd:"test",roles:["dbOwner","userAdmin","readWrite"]} )
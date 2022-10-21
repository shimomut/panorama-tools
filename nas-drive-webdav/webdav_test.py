import os

def test_WebDav( launcher ):

    # See : https://pypi.org/project/webdavclient3/
    
    from webdav3.client import Client

    if 1: # HTTP
        options = {
            'webdav_hostname': "http://10.0.0.246:5005",
            'webdav_login':    "crftwr",
            'webdav_password': "DAXZ4RiXgyvPu8r",
            'disable_check' : True,
        }
    
        client = Client(options)

    elif 0: # HTTPS - still doesn't work
        options = {
            'webdav_hostname': "https://10.0.0.246:5006",
            'webdav_login':    "crftwr",
            'webdav_password': "DAXZ4RiXgyvPu8r",
            'cert_path':       "/etc/ssl/certs/ca-certificates.crt",
            #'cert_path':       "./ssl-cert/cert.pem",
            #'key_path':        "./ssl-cert/privkey.pem",
            #'disable_check' : True,
        }
    
        client = Client(options)

        client.verify = False

    else:
        assert False

    # list directory
    r = client.list("test1")
    print( r, flush=True )

    # make directory
    r = client.mkdir("test1/test2")
    print( r, flush=True )
    
    # confirm new directory created
    r = client.list("test1")
    print( r, flush=True )
    
    # create a temp file
    with open( "/tmp/test3.txt", "w" ) as fd:
        fd.write( "This is test3.txt" )
    
    # upload file
    r = client.upload_sync( local_path="/tmp/test3.txt", remote_path="test1/test2/test3.txt" )
    print( r, flush=True )

    # confirm file uploaded
    r = client.list("test1/test2")
    print( r, flush=True )
    
    # download file
    r = client.download_sync( remote_path="test1/test2/test3.txt", local_path="/tmp/test4.py" )
    print( r, flush=True )
    
    # confirm file downloaded    
    r = os.listdir( "/tmp" )
    print( r, flush=True )

    # delete directory
    r = client.clean("test1/test2")
    print( r, flush=True )
    
    # list directory again to confirm directory deletd
    r = client.list("test1")
    print( r, flush=True )
    

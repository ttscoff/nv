//
//  SyncResponseFetcher.m
//  Notation
//
//  Created by Zachary Schneirov on 11/29/09.

/*Copyright (c) 2010, Zachary Schneirov. All rights reserved.
  Redistribution and use in source and binary forms, with or without modification, are permitted 
  provided that the following conditions are met:
   - Redistributions of source code must retain the above copyright notice, this list of conditions 
     and the following disclaimer.
   - Redistributions in binary form must reproduce the above copyright notice, this list of 
	 conditions and the following disclaimer in the documentation and/or other materials provided with
     the distribution.
   - Neither the name of Notational Velocity nor the names of its contributors may be used to endorse 
     or promote products derived from this software without specific prior written permission. */


#import "SyncResponseFetcher.h"
#import "NSData_transformations.h"

@implementation SyncResponseFetcher

+ (void)initialize {
	
	static BOOL initialized = NO;
    [super initialize];
    if (!initialized) {
		[[NSURLCache sharedURLCache] setDiskCapacity:0];
    }
}


- (id)initWithURL:(NSURL*)aURL bodyStringAsUTF8B64:(NSString*)stringToEncode delegate:(id)aDelegate {
	NSData *B64Data = nil;
	if (stringToEncode) {
		NSString *B64String = [[stringToEncode dataUsingEncoding:NSUTF8StringEncoding] encodeBase64WithNewlines:NO];
		if (!(B64Data = [B64String dataUsingEncoding:NSASCIIStringEncoding])) {
			return nil;
		}
	}
	
	return [self initWithURL:aURL POSTData:B64Data delegate:aDelegate];
}

- (id)initWithURL:(NSURL*)aURL POSTData:(NSData*)POSTData headers:(NSDictionary *)aHeaders delegate:(id)aDelegate {
	return [self initWithURL:aURL POSTData:POSTData headers:aHeaders contentType:nil delegate:aDelegate];
}

- (id)initWithURL:(NSURL*)aURL POSTData:(NSData*)POSTData delegate:(id)aDelegate {
	return [self initWithURL:aURL POSTData:POSTData headers:nil contentType:nil delegate:aDelegate];
}

- (id)initWithURL:(NSURL*)aURL POSTData:(NSData*)POSTData headers:(NSDictionary *)aHeaders contentType:(NSString*)contentType delegate:(id)aDelegate {
	if (self=[self init]) {
		receivedData = [[NSMutableData alloc] init];
		requestURL = [aURL retain];
		delegate = aDelegate;
		dataToSend = [POSTData retain];
		dataToSendContentType = [contentType copy];
		requestHeaders = [aHeaders retain];
        return self;
	}
    return nil;
}

- (NSInvocation*)successInvocation {
	return successInvocation;
}

- (void)setRepresentedObject:(id)anObject {
	[representedObject autorelease];
	representedObject = [anObject retain];
}

- (id)representedObject {
	return representedObject;
}

- (BOOL)startWithSuccessInvocation:(NSInvocation*)anInvocation {

	successInvocation = [anInvocation retain];
	return [self start];
}

- (BOOL)start {
	
	if (isRunning) return YES;
	
	NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:requestURL cachePolicy:NSURLRequestReloadIgnoringCacheData timeoutInterval:30.0];
	if (!request) {
		NSLog(@"%@: Couldn't create HTTP request with URL %@", NSStringFromSelector(_cmd), requestURL);
		return NO;
	}
	
	[request setHTTPShouldHandleCookies:NO];
	[request addValue:@"Sinus cardinalis nvALT 2.2B" forHTTPHeaderField:@"User-agent"];
	
	if (requestHeaders) {
		for (NSString *field in [requestHeaders allKeys]) {
			[request addValue:[requestHeaders objectForKey:field] forHTTPHeaderField:field];
		}
	}
	//if POSTData is nil, do a plain GET request
	if (dataToSend) {
		if (dataToSendContentType) {
			[request addValue:dataToSendContentType forHTTPHeaderField:@"Content-Type"];
		}
		
		[request setHTTPBody:dataToSend];
		[request setHTTPMethod:@"POST"];
	}
	
	[self retain];
	[delegate retain];
	
	didCancel = NO;
	isRunning = YES;
	
	//NSLog(@"starting request for URL '%@'", requestURL);
	if (!(urlConnection = [[NSURLConnection alloc] initWithRequest:request delegate:self])) {
		NSLog(@"%@: Couldn't create NSURLConnection with URLRequest %@", NSStringFromSelector(_cmd), request);
		isRunning = NO;
		[self release];
		[delegate release];
		return NO;
	}
	
	return YES;
}

- (BOOL)isRunning {
	return isRunning;
}

- (BOOL)didCancel {
	return didCancel;
}

- (void)cancel {
	if (!isRunning) return;
	
	didCancel = YES;
	
	[urlConnection cancel];
	[self _fetchDidFinishWithError:NSLocalizedString(@"Operation cancelled", 
													 @"Error string returned to indicate that the user cancelled a syncing service operation")];
}

- (void)dealloc {
	
	[dataToSend release];
	[dataToSendContentType release];
	[requestURL release];
	[receivedData release];
	[urlConnection release];
	[headers release];
	[representedObject release];
	
	[super dealloc];
}

- (id)delegate {
	return delegate;
}

- (NSURL*)requestURL {
	return requestURL;
}

- (NSDictionary*)headers {
	return headers;
}

- (NSInteger)statusCode {
	return lastStatusCode;
}

- (NSString*)errorMessage {
	return lastErrorMessage;
}

- (NSString*)description {
	return [NSString stringWithFormat:@"Fetcher(%X, %@)", self, requestURL];
}

- (void)_fetchDidFinishWithError:(NSString*)anErrString {
	
	if (!isRunning) {
		NSLog(@"not processing %@ because fetcher was already stopped; should not be called", NSStringFromSelector(_cmd));
		return;
	}
	//assumes that anErrString will always be provided in the case of any error, and thus indicates the presence of such
	[delegate syncResponseFetcher:self receivedData:anErrString ? nil : receivedData returningError:anErrString];

	//delegate just had another opportunity to stop us, so check to see if he did that
	if (!isRunning) {
		return;
	}
	
	if (!anErrString) [successInvocation invoke];
	
	[successInvocation autorelease];
	successInvocation = nil;
	
	[lastErrorMessage autorelease];
	lastErrorMessage = [anErrString retain];
	
	[receivedData setLength:0];
	
	[urlConnection autorelease];
	urlConnection = nil;
	isRunning = NO;
	
	[self autorelease];
	[delegate autorelease];
}

- (void)connection:(NSURLConnection *)connection didReceiveResponse:(NSURLResponse *)response {
	[receivedData setLength:0];
	
	lastStatusCode = 0;
	BOOL responseValid = [response isKindOfClass:[NSHTTPURLResponse class]];
	lastStatusCode = [(NSHTTPURLResponse*)response statusCode];
	if (!responseValid) {
		[urlConnection cancel];
		[self _fetchDidFinishWithError:[NSHTTPURLResponse localizedStringForStatusCode:lastStatusCode]];
	} else if (responseValid) {
		[headers autorelease];
		headers = [[(NSHTTPURLResponse*)response allHeaderFields] copy];
	}
}

- (void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)data {
	if (data) {
		[receivedData appendData:data];
	}
}

- (void)connectionDidFinishLoading:(NSURLConnection *)connection {
	[self _fetchDidFinishWithError:nil];
}

- (void)connection:(NSURLConnection *)connection didFailWithError:(NSError *)error {
	[self _fetchDidFinishWithError:[error localizedDescription]];
}

@end

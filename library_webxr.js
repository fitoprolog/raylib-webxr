var LibraryWebXR = {

$WebXR: {
    _coordinateSystem: null,
    _curRAF: null,
    _handTrackingSupported: false,
    _selectCallback: null,
    _selectStartCallback: null,
    _selectEndCallback: null,
    _selectUserData: null,
    _selectStartUserData: null,
    _selectEndUserData: null,
    
    // WebXR Hand Joint indices (25 joints per hand)
    _HAND_JOINTS: [
        'wrist',
        'thumb-metacarpal', 'thumb-phalanx-proximal', 'thumb-phalanx-distal', 'thumb-tip',
        'index-finger-metacarpal', 'index-finger-phalanx-proximal', 'index-finger-phalanx-intermediate', 'index-finger-phalanx-distal', 'index-finger-tip',
        'middle-finger-metacarpal', 'middle-finger-phalanx-proximal', 'middle-finger-phalanx-intermediate', 'middle-finger-phalanx-distal', 'middle-finger-tip',
        'ring-finger-metacarpal', 'ring-finger-phalanx-proximal', 'ring-finger-phalanx-intermediate', 'ring-finger-phalanx-distal', 'ring-finger-tip',
        'pinky-finger-metacarpal', 'pinky-finger-phalanx-proximal', 'pinky-finger-phalanx-intermediate', 'pinky-finger-phalanx-distal', 'pinky-finger-tip'
    ],

    _nativize_vec3: function(offset, vec) {
        setValue(offset    , vec[0], 'float');
        setValue(offset + 4, vec[1], 'float');
        setValue(offset + 8, vec[2], 'float');

        return offset + 12;
    },

    _nativize_matrix: function(offset, mat) {
        for (var i = 0; i < 16; ++i) {
            setValue(offset + i*4, mat[i], 'float');
        }

        return offset + 16*4;
    },
    
    _nativize_hand_joints: function(offset, hand, frame, coordinateSystem) {
        if (!hand || !frame || !coordinateSystem) {
            // Fill with zeros if no hand data
            for (let i = 0; i < 25; i++) {
                // Position (3 floats)
                setValue(offset + i * 32, 0.0, 'float');
                setValue(offset + i * 32 + 4, 0.0, 'float');
                setValue(offset + i * 32 + 8, 0.0, 'float');
                // Rotation (4 floats)
                setValue(offset + i * 32 + 12, 0.0, 'float');
                setValue(offset + i * 32 + 16, 0.0, 'float');
                setValue(offset + i * 32 + 20, 0.0, 'float');
                setValue(offset + i * 32 + 24, 1.0, 'float');
                // Radius (1 float)
                setValue(offset + i * 32 + 28, 0.0, 'float');
            }
            return offset + 25 * 32;
        }
        
        for (let i = 0; i < WebXR._HAND_JOINTS.length && i < 25; i++) {
            const jointName = WebXR._HAND_JOINTS[i];
            try {
                const jointPose = frame.getJointPose(hand.get(jointName), coordinateSystem);
                
                if (jointPose) {
                    const pos = jointPose.transform.position;
                    const rot = jointPose.transform.orientation;
                    
                    // Position (3 floats)
                    setValue(offset + i * 32, pos.x, 'float');
                    setValue(offset + i * 32 + 4, pos.y, 'float');
                    setValue(offset + i * 32 + 8, pos.z, 'float');
                    
                    // Rotation (4 floats - quaternion)
                    setValue(offset + i * 32 + 12, rot.x, 'float');
                    setValue(offset + i * 32 + 16, rot.y, 'float');
                    setValue(offset + i * 32 + 20, rot.z, 'float');
                    setValue(offset + i * 32 + 24, rot.w, 'float');
                    
                    // Radius (1 float)
                    setValue(offset + i * 32 + 28, jointPose.radius || 0.01, 'float');
                } else {
                    // Fill with zeros if joint pose is not available
                    for (let j = 0; j < 8; j++) {
                        setValue(offset + i * 32 + j * 4, 0.0, 'float');
                    }
                    setValue(offset + i * 32 + 24, 1.0, 'float'); // w component of quaternion
                }
            } catch (e) {
                console.warn(`Failed to get pose for joint ${jointName}:`, e);
                // Fill with zeros on error
                for (let j = 0; j < 8; j++) {
                    setValue(offset + i * 32 + j * 4, 0.0, 'float');
                }
                setValue(offset + i * 32 + 24, 1.0, 'float'); // w component of quaternion
            }
        }
        
        return offset + 25 * 32;
    },
    /* Sets input source values to offset and returns pointer after struct */
    _nativize_input_source: function(offset, inputSource, id) {
        var handedness = -1;
        if(inputSource.handedness == "left") handedness = 0;
        else if(inputSource.handedness == "right") handedness = 1;

        var targetRayMode = 0;
        if(inputSource.targetRayMode == "tracked-pointer") targetRayMode = 1;
        else if(inputSource.targetRayMode == "screen") targetRayMode = 2;

        var hasHand = inputSource.hand ? 1 : 0;
        var hasController = (inputSource.gamepad || inputSource.targetRaySpace) ? 1 : 0;

        setValue(offset, id, 'i32');
        offset +=4;
        setValue(offset, handedness, 'i32');
        offset +=4;
        setValue(offset, targetRayMode, 'i32');
        offset +=4;
        setValue(offset, hasHand, 'i32');
        offset +=4;
        setValue(offset, hasController, 'i32');
        offset +=4;

        return offset;
    },

    _set_input_callback: function(event, callback, userData) {
        var s = Module['webxr_session'];
        if(!s) return;
        if(!callback) return;

        s.addEventListener(event, function(e) {
            console.log(`WebXR input event: ${event}`);
            
            /* Allocate memory for WebXRInputSource struct (5 int32s) */
            var inputSourceStruct = Module._malloc(20); // 5*sizeof(int32)
            
            /* Find the input source index */
            var inputSourceIndex = -1;
            for (let i = 0; i < s.inputSources.length; i++) {
                if (s.inputSources[i] === e.inputSource) {
                    inputSourceIndex = i;
                    break;
                }
            }
            
            if (inputSourceIndex >= 0) {
                WebXR._nativize_input_source(inputSourceStruct, e.inputSource, inputSourceIndex);

                /* Call native callback */
                dynCall('vii', callback, [inputSourceStruct, userData]);
            } else {
                console.warn('Could not find input source index for event');
            }

            Module._free(inputSourceStruct);
        });
    },

    _set_session_callback: function(event, callback, userData) {
        var s = Module['webxr_session'];
        if(!s) return;
        if(!callback) return;

        s.addEventListener(event, function() {
            dynCall('vi', callback, [userData]);
        });
    }
},

webxr_init: function(mode, frameCallback, startSessionCallback, endSessionCallback, errorCallback, userData) {
    function onError(errorCode) {
        if(!errorCallback) return;
        dynCall('vii', errorCallback, [userData, errorCode]);
    };

    function onSessionEnd(session) {
        if(!endSessionCallback) return;
        dynCall('vi', endSessionCallback, [userData]);
    };

    function onSessionStart() {
        if(!startSessionCallback) return;
        dynCall('vi', startSessionCallback, [userData]);
    };
    
    // Store session mode for later use
    Module['webxr_session_mode'] = mode;

    function onFrame(time, frame) {
        if(!frameCallback) return;
        /* Request next frame */
        const session = frame.session;
        /* RAF is set to null on session end to avoid rendering */
        if(Module['webxr_session'] != null) session.requestAnimationFrame(onFrame);

        const pose = frame.getViewerPose(WebXR._coordinateSystem);
        if(!pose) return;

        const SIZE_OF_WEBXR_VIEW = (16 + 16 + 4+7)*4;
        const views = Module._malloc(SIZE_OF_WEBXR_VIEW*2 + 16*4);

        const glLayer = session.renderState.baseLayer;
        window.glLayer = glLayer;
        pose.views.forEach(function(view) {
            const viewport = glLayer.getViewport(view);
            const viewMatrix = view.transform.matrix;//inverse.matrix;
            let offset = views + SIZE_OF_WEBXR_VIEW*(view.eye == 'left' ? 0 : 1);

            offset = WebXR._nativize_matrix(offset, viewMatrix);
            offset = WebXR._nativize_matrix(offset, view.projectionMatrix);

            setValue(offset    , viewport.x, 'i32');
            setValue(offset + 4, viewport.y, 'i32');
            setValue(offset + 8, viewport.width, 'i32');
            setValue(offset + 12, viewport.height, 'i32');
            offset += 16;
            const p=view.transform.position;
            offset = WebXR._nativize_vec3(offset,[p.x,p.y,p.z]);
            const r=view.transform.orientation;
            setValue(offset,    r.x,'float');
            setValue(offset+4,  r.y,'float');
            setValue(offset+8,  r.z,'float');
            setValue(offset+12, r.w,'float');
        });

        /* Model matrix */
        const modelMatrix = views + SIZE_OF_WEBXR_VIEW*2;
        WebXR._nativize_matrix(modelMatrix, pose.transform.matrix);
        
        // Allocate memory for hand tracking data (2 hands * 25 joints * 32 bytes per joint)
        const handDataSize = 2 * 25 * 32;
        const handData = Module._malloc(handDataSize + 8); // +8 for hand detection flags
        
        // Check for hand tracking support and collect hand data
        let leftHandDetected = 0;
        let rightHandDetected = 0;
        
        for (const inputSource of session.inputSources) {
            if (inputSource.hand) {
                if (inputSource.handedness === 'left') {
                    leftHandDetected = 1;
                    WebXR._nativize_hand_joints(handData, inputSource.hand, frame, WebXR._coordinateSystem);
                } else if (inputSource.handedness === 'right') {
                    rightHandDetected = 1;
                    WebXR._nativize_hand_joints(handData + 25 * 32, inputSource.hand, frame, WebXR._coordinateSystem);
                }
            }
        }
        
        // Set hand detection flags at the end of hand data
        setValue(handData + handDataSize, leftHandDetected, 'i32');
        setValue(handData + handDataSize + 4, rightHandDetected, 'i32');
        
        Module.ctx.bindFramebuffer(Module.ctx.FRAMEBUFFER,
            glLayer.framebuffer);
        /* HACK: This is not generally necessary, but chrome seems to detect whether the
         * page is sending frames by waiting for depth buffer clear or something */
        // TODO still necessary?
        Module.ctx.clear(Module.ctx.DEPTH_BUFFER_BIT);

        /* Set and reset environment for webxr_get_input_pose calls */
        Module['webxr_frame'] = frame;
        dynCall('viiiii', frameCallback, [userData, time, modelMatrix, views, handData]);
        Module['webxr_frame'] = null;

        _free(views);
        _free(handData);
    };

    function onSessionStarted(session) {
        Module['webxr_session'] = session;
        Module['webxr_on_session_started'] = onSessionStarted;

        // React to session ending
        session.addEventListener('end', function() {
            Module['webxr_session'].cancelAnimationFrame(WebXR._curRAF);
            WebXR._curRAF = null;
            Module['webxr_session'] = null;
            onSessionEnd();
        });

        // Give application a chance to react to session starting
        // e.g. finish current desktop frame.
        onSessionStart();
        
        // Register input callbacks if they were set before session started
        if (WebXR._selectCallback) {
            WebXR._set_input_callback('select', WebXR._selectCallback, WebXR._selectUserData);
        }
        if (WebXR._selectStartCallback) {
            WebXR._set_input_callback('selectstart', WebXR._selectStartCallback, WebXR._selectStartUserData);
        }
        if (WebXR._selectEndCallback) {
            WebXR._set_input_callback('selectend', WebXR._selectEndCallback, WebXR._selectEndUserData);
        }

        // Ensure our context can handle WebXR rendering
        Module.ctx.makeXRCompatible().then(function() {
            // Create the base layer
            session.updateRenderState({
                baseLayer: new XRWebGLLayer(session, Module.ctx)
            });

            session.requestReferenceSpace('local').then(refSpace => {
                WebXR._coordinateSystem = refSpace;
                // Start rendering
                session.requestAnimationFrame(onFrame);
            });
        }, function(err) {
            onError(-3);
        });
    };

    if(navigator.xr) {
        const sessionModes = ['inline', 'immersive-vr', 'immersive-ar'];
        const sessionMode = sessionModes[mode];
        
        // Check if XR session is supported
        navigator.xr.isSessionSupported(sessionMode).then(function() {
            Module['webxr_request_session_func'] = function() {
                // Define required features based on session mode
                let requiredFeatures = [];
                let optionalFeatures = ['hand-tracking'];
                
                if (sessionMode === 'immersive-ar') {
                    optionalFeatures.push('hit-test', 'plane-detection');
                }
                
                const sessionInit = {
                    requiredFeatures: requiredFeatures,
                    optionalFeatures: optionalFeatures
                };
                
                navigator.xr.requestSession(sessionMode, sessionInit).then(onSessionStarted).catch(function(err) {
                    console.warn('Failed to start XR session with optional features, trying without:', err);
                    // Fallback without optional features
                    navigator.xr.requestSession(sessionMode, {requiredFeatures: requiredFeatures}).then(onSessionStarted).catch(function(err2) {
                        console.error('Failed to start XR session:', err2);
                        onError(-4);
                    });
                });
            };
        }, function() {
            onError(-4);
        });
    } else {
        /* Call error callback with "WebXR not supported" */
        onError(-2);
    }
},

webxr_request_session: function() {
    var s = Module['webxr_request_session_func'];
    if(s) Module['webxr_request_session_func']();
},

webxr_request_exit: function() {
    var s = Module['webxr_session'];
    if(s) Module['webxr_session'].end();
},

webxr_set_projection_params: function(near, far) {
    var s = Module['webxr_session'];
    if(!s) return;

    s.depthNear = near;
    s.depthFar = far;
},

webxr_set_session_blur_callback: function(callback, userData) {
    WebXR._set_session_callback("blur", callback, userData);
},

webxr_set_session_focus_callback: function(callback, userData) {
    WebXR._set_session_callback("focus", callback, userData);
},

webxr_set_select_callback: function(callback, userData) {
    WebXR._selectCallback = callback;
    WebXR._selectUserData = userData;
    // If session already exists, register immediately
    if (Module['webxr_session']) {
        WebXR._set_input_callback("select", callback, userData);
    }
},
webxr_set_select_start_callback: function(callback, userData) {
    WebXR._selectStartCallback = callback;
    WebXR._selectStartUserData = userData;
    // If session already exists, register immediately
    if (Module['webxr_session']) {
        WebXR._set_input_callback("selectstart", callback, userData);
    }
},
webxr_set_select_end_callback: function(callback, userData) {
    WebXR._selectEndCallback = callback;
    WebXR._selectEndUserData = userData;
    // If session already exists, register immediately
    if (Module['webxr_session']) {
        WebXR._set_input_callback("selectend", callback, userData);
    }
},

webxr_get_input_sources: function(outArrayPtr, max, outCountPtr) {
    var s = Module['webxr_session'];
    if(!s) return; // TODO(squareys) warning or return error

    var i = 0;
    for (let inputSource of s.inputSources) {
        if(i >= max) break;
        outArrayPtr = WebXR._nativize_input_source(outArrayPtr, inputSource, i);
        ++i;
    }
    setValue(outCountPtr, i, 'i32');
},

webxr_get_input_pose: function(source, outPosePtr) {
    var f = Module['webxr_frame'];
    if(!f) {
        console.warn("Cannot call webxr_get_input_pose outside of frame callback");
        return;
    }

    const id = getValue(source, 'i32');
    const input = Module['webxr_session'].inputSources[id];

    pose = f.getPose(input.gripSpace, WebXR._coordinateSystem);

    offset = outPosePtr;
    /* WebXRRay */
    offset = WebXR._nativize_matrix(offset, pose.transform.matrix);

    /* WebXRInputPose */
    //offset = WebXR._nativize_matrix(offset, pose.gripMatrix);
    //setValue(offset, pose.emulatedPosition, 'i32');
},

webxr_is_hand_tracking_supported: function() {
    var s = Module['webxr_session'];
    if (!s) return 0;
    
    // Check if any input source has hand tracking
    for (const inputSource of s.inputSources) {
        if (inputSource.hand) {
            return 1;
        }
    }
    return 0;
},

webxr_get_hand_joint_pose: function(handedness, jointIndex, outPosePtr) {
    var f = Module['webxr_frame'];
    if (!f) return 0;
    
    var s = Module['webxr_session'];
    if (!s) return 0;
    
    const handednessStr = handedness === 0 ? 'left' : 'right';
    
    for (const inputSource of s.inputSources) {
        if (inputSource.handedness === handednessStr && inputSource.hand) {
            if (jointIndex >= 0 && jointIndex < WebXR._HAND_JOINTS.length) {
                const jointName = WebXR._HAND_JOINTS[jointIndex];
                try {
                    const jointPose = f.getJointPose(inputSource.hand.get(jointName), WebXR._coordinateSystem);
                    if (jointPose) {
                        const pos = jointPose.transform.position;
                        const rot = jointPose.transform.orientation;
                        
                        // Position (3 floats)
                        setValue(outPosePtr, pos.x, 'float');
                        setValue(outPosePtr + 4, pos.y, 'float');
                        setValue(outPosePtr + 8, pos.z, 'float');
                        
                        // Rotation (4 floats - quaternion)
                        setValue(outPosePtr + 12, rot.x, 'float');
                        setValue(outPosePtr + 16, rot.y, 'float');
                        setValue(outPosePtr + 20, rot.z, 'float');
                        setValue(outPosePtr + 24, rot.w, 'float');
                        
                        // Radius (1 float)
                        setValue(outPosePtr + 28, jointPose.radius || 0.01, 'float');
                        
                        return 1; // Success
                    }
                } catch (e) {
                    console.warn(`Failed to get pose for joint ${jointName}:`, e);
                }
            }
            break;
        }
    }
    return 0; // Failed
},

webxr_is_ar_session: function() {
    return Module['webxr_session_mode'] === 2 ? 1 : 0; // WEBXR_SESSION_MODE_IMMERSIVE_AR = 2
},

webxr_request_ar_session: function() {
    if (navigator.xr) {
        const sessionInit = {
            requiredFeatures: [],
            optionalFeatures: ['hit-test', 'plane-detection', 'hand-tracking']
        };
        
        navigator.xr.requestSession('immersive-ar', sessionInit).then(function(session) {
            Module['webxr_session_mode'] = 2;
            
            // Call onSessionStarted directly if available, otherwise use the stored function
            if (typeof onSessionStarted === 'function') {
                onSessionStarted(session);
            } else if (Module['webxr_on_session_started']) {
                Module['webxr_on_session_started'](session);
            } else {
                console.warn('No session started handler available');
            }
        }).catch(function(err) {
            console.error('Failed to start AR session:', err);
        });
    }
},

};

autoAddDeps(LibraryWebXR, '$WebXR');
mergeInto(LibraryManager.library, LibraryWebXR);

{
  "fileFormat": 2,
  "featureLevel": 107,
  "creator": "zap",
  "keyValuePairs": [
    {
      "key": "commandDiscovery",
      "value": "1"
    },
    {
      "key": "defaultResponsePolicy",
      "value": "always"
    },
    {
      "key": "manufacturerCodes",
      "value": "0x1002"
    }
  ],
  "package": [
    {
      "pathRelativity": "relativeToZap",
      "path": "..\\..\\..\\..\\SDKs\\simplicity_sdk_2\\app\\zcl\\zcl-zap.json",
      "type": "zcl-properties",
      "category": "zigbee",
      "version": 1,
      "description": "Zigbee Silabs ZCL data"
    },
    {
      "pathRelativity": "relativeToZap",
      "path": "..\\..\\..\\..\\SDKs\\simplicity_sdk_2\\protocol\\zigbee\\app\\framework\\gen-template\\gen-templates.json",
      "type": "gen-templates-json",
      "category": "zigbee",
      "version": "zigbee-v0"
    }
  ],
  "endpointTypes": [
    {
      "id": 1,
      "name": "Anonymous Endpoint Type",
      "deviceTypeRef": {
        "code": 257,
        "profileId": 260,
        "label": "LO-dimmablelight",
        "name": "LO-dimmablelight",
        "deviceTypeOrder": 0
      },
      "deviceTypes": [
        {
          "code": 257,
          "profileId": 260,
          "label": "LO-dimmablelight",
          "name": "LO-dimmablelight",
          "deviceTypeOrder": 0
        }
      ],
      "deviceVersions": [
        1
      ],
      "deviceIdentifiers": [
        257
      ],
      "deviceTypeName": "LO-dimmablelight",
      "deviceTypeCode": 257,
      "deviceTypeProfileId": 260,
      "clusters": [
        {
          "name": "Basic",
          "code": 0,
          "mfgCode": null,
          "define": "BASIC_CLUSTER",
          "side": "server",
          "enabled": 1,
          "attributes": [
            {
              "name": "ZCL version",
              "code": 0,
              "mfgCode": null,
              "side": "server",
              "type": "int8u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": null,
              "defaultValue": "8",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "application version",
              "code": 1,
              "mfgCode": null,
              "side": "server",
              "type": "int8u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": null,
              "defaultValue": "0",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "stack version",
              "code": 2,
              "mfgCode": null,
              "side": "server",
              "type": "int8u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": null,
              "defaultValue": "82",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "hardware version",
              "code": 3,
              "mfgCode": null,
              "side": "server",
              "type": "int8u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": null,
              "defaultValue": "1",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "manufacturer name",
              "code": 4,
              "mfgCode": null,
              "side": "server",
              "type": "char_string",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": 0,
              "defaultValue": "Adaptiv Ltd",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "model identifier",
              "code": 5,
              "mfgCode": null,
              "side": "server",
              "type": "char_string",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": 0,
              "defaultValue": "NT-3",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "date code",
              "code": 6,
              "mfgCode": null,
              "side": "server",
              "type": "char_string",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": null,
              "defaultValue": "20250101",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "power source",
              "code": 7,
              "mfgCode": null,
              "side": "server",
              "type": "enum8",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": null,
              "defaultValue": "1",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "generic device class",
              "code": 8,
              "mfgCode": null,
              "side": "server",
              "type": "enum8",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": null,
              "defaultValue": "0",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "generic device type",
              "code": 9,
              "mfgCode": null,
              "side": "server",
              "type": "enum8",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": null,
              "defaultValue": "9",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "product code",
              "code": 10,
              "mfgCode": null,
              "side": "server",
              "type": "octet_string",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": null,
              "defaultValue": "NT-3.0",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "product url",
              "code": 11,
              "mfgCode": null,
              "side": "server",
              "type": "char_string",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": null,
              "defaultValue": "0",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "sw build id",
              "code": 16384,
              "mfgCode": null,
              "side": "server",
              "type": "char_string",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": 0,
              "defaultValue": "v00.00.02",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "cluster revision",
              "code": 65533,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": null,
              "defaultValue": "3",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            }
          ]
        },
        {
          "name": "Identify",
          "code": 3,
          "mfgCode": null,
          "define": "IDENTIFY_CLUSTER",
          "side": "client",
          "enabled": 1,
          "commands": [
            {
              "name": "Identify",
              "code": 0,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "IdentifyQueryResponse",
              "code": 0,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "IdentifyQuery",
              "code": 1,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 0,
              "isEnabled": 1
            }
          ],
          "attributes": [
            {
              "name": "cluster revision",
              "code": 65533,
              "mfgCode": null,
              "side": "client",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": null,
              "defaultValue": "2",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            }
          ]
        },
        {
          "name": "Identify",
          "code": 3,
          "mfgCode": null,
          "define": "IDENTIFY_CLUSTER",
          "side": "server",
          "enabled": 1,
          "commands": [
            {
              "name": "IdentifyQueryResponse",
              "code": 0,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "Identify",
              "code": 0,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "IdentifyQuery",
              "code": 1,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "TriggerEffect",
              "code": 64,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            }
          ],
          "attributes": [
            {
              "name": "identify time",
              "code": 0,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": null,
              "defaultValue": "60",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "cluster revision",
              "code": 65533,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": null,
              "defaultValue": "2",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            }
          ]
        },
        {
          "name": "Groups",
          "code": 4,
          "mfgCode": null,
          "define": "GROUPS_CLUSTER",
          "side": "server",
          "enabled": 1,
          "commands": [
            {
              "name": "AddGroup",
              "code": 0,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "AddGroupResponse",
              "code": 0,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "ViewGroupResponse",
              "code": 1,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "ViewGroup",
              "code": 1,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "GetGroupMembership",
              "code": 2,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "GetGroupMembershipResponse",
              "code": 2,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "RemoveGroupResponse",
              "code": 3,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "RemoveGroup",
              "code": 3,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "RemoveAllGroups",
              "code": 4,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "AddGroupIfIdentifying",
              "code": 5,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            }
          ],
          "attributes": [
            {
              "name": "name support",
              "code": 0,
              "mfgCode": null,
              "side": "server",
              "type": "bitmap8",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": null,
              "defaultValue": "128",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "cluster revision",
              "code": 65533,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": null,
              "defaultValue": "3",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            }
          ]
        },
        {
          "name": "Scenes",
          "code": 5,
          "mfgCode": null,
          "define": "SCENES_CLUSTER",
          "side": "server",
          "enabled": 1,
          "commands": [
            {
              "name": "AddSceneResponse",
              "code": 0,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "AddScene",
              "code": 0,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "ViewScene",
              "code": 1,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "ViewSceneResponse",
              "code": 1,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "RemoveScene",
              "code": 2,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "RemoveSceneResponse",
              "code": 2,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "RemoveAllScenesResponse",
              "code": 3,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "RemoveAllScenes",
              "code": 3,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "StoreScene",
              "code": 4,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "StoreSceneResponse",
              "code": 4,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "RecallScene",
              "code": 5,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "GetSceneMembership",
              "code": 6,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "GetSceneMembershipResponse",
              "code": 6,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "EnhancedAddScene",
              "code": 64,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "EnhancedViewScene",
              "code": 65,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "CopyScene",
              "code": 66,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            }
          ],
          "attributes": [
            {
              "name": "scene count",
              "code": 0,
              "mfgCode": null,
              "side": "server",
              "type": "int8u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x00",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "current scene",
              "code": 1,
              "mfgCode": null,
              "side": "server",
              "type": "int8u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x00",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "current group",
              "code": 2,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x0000",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "scene valid",
              "code": 3,
              "mfgCode": null,
              "side": "server",
              "type": "boolean",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x00",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "name support",
              "code": 4,
              "mfgCode": null,
              "side": "server",
              "type": "bitmap8",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x00",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "cluster revision",
              "code": 65533,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "3",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            }
          ]
        },
        {
          "name": "On/off",
          "code": 6,
          "mfgCode": null,
          "define": "ON_OFF_CLUSTER",
          "side": "server",
          "enabled": 1,
          "commands": [
            {
              "name": "Off",
              "code": 0,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "On",
              "code": 1,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "Toggle",
              "code": 2,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "OffWithEffect",
              "code": 64,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "OnWithRecallGlobalScene",
              "code": 65,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "OnWithTimedOff",
              "code": 66,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            }
          ],
          "attributes": [
            {
              "name": "on/off",
              "code": 0,
              "mfgCode": null,
              "side": "server",
              "type": "boolean",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x01",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "global scene control",
              "code": 16384,
              "mfgCode": null,
              "side": "server",
              "type": "boolean",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x01",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "on time",
              "code": 16385,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x0000",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "off wait time",
              "code": 16386,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x0000",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "start up on off",
              "code": 16387,
              "mfgCode": null,
              "side": "server",
              "type": "enum8",
              "included": 1,
              "storageOption": "NVM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "1",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "cluster revision",
              "code": 65533,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "2",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            }
          ]
        },
        {
          "name": "Level Control",
          "code": 8,
          "mfgCode": null,
          "define": "LEVEL_CONTROL_CLUSTER",
          "side": "server",
          "enabled": 1,
          "commands": [
            {
              "name": "MoveToLevel",
              "code": 0,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "Move",
              "code": 1,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "Step",
              "code": 2,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "Stop",
              "code": 3,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "MoveToLevelWithOnOff",
              "code": 4,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "MoveWithOnOff",
              "code": 5,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "StepWithOnOff",
              "code": 6,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "StopWithOnOff",
              "code": 7,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            }
          ],
          "attributes": [
            {
              "name": "current level",
              "code": 0,
              "mfgCode": null,
              "side": "server",
              "type": "int8u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x00",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "remaining time",
              "code": 1,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x0000",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "min level",
              "code": 2,
              "mfgCode": null,
              "side": "server",
              "type": "int8u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "max level",
              "code": 3,
              "mfgCode": null,
              "side": "server",
              "type": "int8u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0xFE",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "options",
              "code": 15,
              "mfgCode": null,
              "side": "server",
              "type": "bitmap8",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x01",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "on level",
              "code": 17,
              "mfgCode": null,
              "side": "server",
              "type": "int8u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0xCD",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "start up current level",
              "code": 16384,
              "mfgCode": null,
              "side": "server",
              "type": "int8u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0xF0",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "cluster revision",
              "code": 65533,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "3",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            }
          ]
        },
        {
          "name": "Commissioning",
          "code": 21,
          "mfgCode": null,
          "define": "COMMISSIONING_CLUSTER",
          "side": "server",
          "enabled": 1,
          "commands": [
            {
              "name": "RestartDeviceResponse",
              "code": 0,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "RestartDevice",
              "code": 0,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "SaveStartupParametersResponse",
              "code": 1,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "RestoreStartupParametersResponse",
              "code": 2,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "ResetStartupParameters",
              "code": 3,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "ResetStartupParametersResponse",
              "code": 3,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 0,
              "isEnabled": 1
            }
          ],
          "attributes": [
            {
              "name": "short address",
              "code": 0,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "NVM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0xDEAD",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "extended pan id",
              "code": 1,
              "mfgCode": null,
              "side": "server",
              "type": "ieee_address",
              "included": 1,
              "storageOption": "NVM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x0FFFFFFFFFFFFFFF",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "pan id",
              "code": 2,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "NVM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0xFFFF",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "channel mask",
              "code": 3,
              "mfgCode": null,
              "side": "server",
              "type": "bitmap32",
              "included": 1,
              "storageOption": "NVM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x07FFF800",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "protocol version",
              "code": 4,
              "mfgCode": null,
              "side": "server",
              "type": "int8u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x02",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "stack profile",
              "code": 5,
              "mfgCode": null,
              "side": "server",
              "type": "int8u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x02",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "startup control",
              "code": 6,
              "mfgCode": null,
              "side": "server",
              "type": "enum8",
              "included": 1,
              "storageOption": "NVM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x00",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "trust center address",
              "code": 16,
              "mfgCode": null,
              "side": "server",
              "type": "ieee_address",
              "included": 1,
              "storageOption": "NVM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x0000000000000000",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "network key",
              "code": 18,
              "mfgCode": null,
              "side": "server",
              "type": "security_key",
              "included": 1,
              "storageOption": "NVM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x00000000000000000000000000000000",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "use insecure join",
              "code": 19,
              "mfgCode": null,
              "side": "server",
              "type": "boolean",
              "included": 1,
              "storageOption": "NVM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x01",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "preconfigured link key",
              "code": 20,
              "mfgCode": null,
              "side": "server",
              "type": "security_key",
              "included": 1,
              "storageOption": "NVM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x00000000000000000000000000000000",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "network key sequence number",
              "code": 21,
              "mfgCode": null,
              "side": "server",
              "type": "int8u",
              "included": 1,
              "storageOption": "NVM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x00",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "network key type",
              "code": 22,
              "mfgCode": null,
              "side": "server",
              "type": "enum8",
              "included": 1,
              "storageOption": "NVM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x05",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "network manager address",
              "code": 23,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "NVM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x0000",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "scan attempts",
              "code": 32,
              "mfgCode": null,
              "side": "server",
              "type": "int8u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x05",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "time between scans",
              "code": 33,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x0064",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "rejoin interval",
              "code": 34,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x003C",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "max rejoin interval",
              "code": 35,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x0E10",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "indirect poll rate",
              "code": 48,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "parent retry threshold",
              "code": 49,
              "mfgCode": null,
              "side": "server",
              "type": "int8u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "concentrator flag",
              "code": 64,
              "mfgCode": null,
              "side": "server",
              "type": "boolean",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x00",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "concentrator radius",
              "code": 65,
              "mfgCode": null,
              "side": "server",
              "type": "int8u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x0F",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "concentrator discovery time",
              "code": 66,
              "mfgCode": null,
              "side": "server",
              "type": "int8u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x00",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "cluster revision",
              "code": 65533,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "2",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            }
          ]
        },
        {
          "name": "Over the Air Bootloading",
          "code": 25,
          "mfgCode": null,
          "define": "OTA_BOOTLOAD_CLUSTER",
          "side": "client",
          "enabled": 1,
          "commands": [
            {
              "name": "ImageNotify",
              "code": 0,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "QueryNextImageRequest",
              "code": 1,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "QueryNextImageResponse",
              "code": 2,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "ImageBlockRequest",
              "code": 3,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "ImageBlockResponse",
              "code": 5,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "UpgradeEndRequest",
              "code": 6,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "UpgradeEndResponse",
              "code": 7,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 1,
              "isEnabled": 1
            }
          ],
          "attributes": [
            {
              "name": "OTA Upgrade Server ID",
              "code": 0,
              "mfgCode": null,
              "side": "client",
              "type": "ieee_address",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0xffffffffffffffff",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "Offset (address) into the file",
              "code": 1,
              "mfgCode": null,
              "side": "client",
              "type": "int32u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0xffffffff",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "OTA Current File Version",
              "code": 2,
              "mfgCode": null,
              "side": "client",
              "type": "int32u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x00000002",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "OTA Current ZigBee Stack Version",
              "code": 3,
              "mfgCode": null,
              "side": "client",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "82",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "OTA Upgrade Status",
              "code": 6,
              "mfgCode": null,
              "side": "client",
              "type": "enum8",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x00",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "Manufacturer ID",
              "code": 7,
              "mfgCode": null,
              "side": "client",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x1574",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "Image Type ID",
              "code": 8,
              "mfgCode": null,
              "side": "client",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0xFFFF",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "cluster revision",
              "code": 65533,
              "mfgCode": null,
              "side": "client",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "4",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            }
          ]
        },
        {
          "name": "Diagnostics",
          "code": 2821,
          "mfgCode": null,
          "define": "DIAGNOSTICS_CLUSTER",
          "side": "server",
          "enabled": 1,
          "attributes": [
            {
              "name": "number of resets",
              "code": 0,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "NVM",
              "singleton": 1,
              "bounded": 0,
              "defaultValue": "0x0000",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "cluster revision",
              "code": 65533,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "3",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            }
          ]
        },
        {
          "name": "Sample Mfg Specific Cluster",
          "code": 64512,
          "mfgCode": 4098,
          "define": "SAMPLE_MFG_SPECIFIC_CLUSTER",
          "side": "server",
          "enabled": 1,
          "attributes": [
            {
              "name": "cluster revision",
              "code": 65533,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x0001",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            }
          ]
        },
        {
          "name": "Configuration Cluster",
          "code": 64513,
          "mfgCode": 4098,
          "define": "OTA_CONFIGURATION_CLUSTER",
          "side": "server",
          "enabled": 1,
          "commands": [
            {
              "name": "SetToken",
              "code": 0,
              "mfgCode": 4098,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "ReturnToken",
              "code": 0,
              "mfgCode": 4098,
              "source": "server",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "LockTokens",
              "code": 1,
              "mfgCode": 4098,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "ReadTokens",
              "code": 2,
              "mfgCode": 4098,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "UnlockTokens",
              "code": 3,
              "mfgCode": 4098,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            }
          ],
          "attributes": [
            {
              "name": "Prevents OTA writing of tokens.",
              "code": 0,
              "mfgCode": 4098,
              "side": "server",
              "type": "boolean",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x01",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "cluster revision",
              "code": 65533,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x0001",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            }
          ]
        }
      ]
    },
    {
      "id": 2,
      "name": "Anonymous Endpoint Type",
      "deviceTypeRef": {
        "code": 263,
        "profileId": 260,
        "label": "LO-occupancysensor",
        "name": "LO-occupancysensor",
        "deviceTypeOrder": 0
      },
      "deviceTypes": [
        {
          "code": 263,
          "profileId": 260,
          "label": "LO-occupancysensor",
          "name": "LO-occupancysensor",
          "deviceTypeOrder": 0
        }
      ],
      "deviceVersions": [
        1
      ],
      "deviceIdentifiers": [
        263
      ],
      "deviceTypeName": "LO-occupancysensor",
      "deviceTypeCode": 263,
      "deviceTypeProfileId": 260,
      "clusters": [
        {
          "name": "Basic",
          "code": 0,
          "mfgCode": null,
          "define": "BASIC_CLUSTER",
          "side": "server",
          "enabled": 1,
          "attributes": [
            {
              "name": "ZCL version",
              "code": 0,
              "mfgCode": null,
              "side": "server",
              "type": "int8u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": null,
              "defaultValue": "8",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "application version",
              "code": 1,
              "mfgCode": null,
              "side": "server",
              "type": "int8u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": null,
              "defaultValue": "0",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "stack version",
              "code": 2,
              "mfgCode": null,
              "side": "server",
              "type": "int8u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": null,
              "defaultValue": "82",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "hardware version",
              "code": 3,
              "mfgCode": null,
              "side": "server",
              "type": "int8u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": null,
              "defaultValue": "1",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "manufacturer name",
              "code": 4,
              "mfgCode": null,
              "side": "server",
              "type": "char_string",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": 0,
              "defaultValue": "Adaptiv Ltd",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "model identifier",
              "code": 5,
              "mfgCode": null,
              "side": "server",
              "type": "char_string",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": 0,
              "defaultValue": "NT-3",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "date code",
              "code": 6,
              "mfgCode": null,
              "side": "server",
              "type": "char_string",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": null,
              "defaultValue": "20250101",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "power source",
              "code": 7,
              "mfgCode": null,
              "side": "server",
              "type": "enum8",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": null,
              "defaultValue": "1",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "generic device class",
              "code": 8,
              "mfgCode": null,
              "side": "server",
              "type": "enum8",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": null,
              "defaultValue": "0",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "generic device type",
              "code": 9,
              "mfgCode": null,
              "side": "server",
              "type": "enum8",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": null,
              "defaultValue": "9",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "product code",
              "code": 10,
              "mfgCode": null,
              "side": "server",
              "type": "octet_string",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": null,
              "defaultValue": "NT-3.0",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "product url",
              "code": 11,
              "mfgCode": null,
              "side": "server",
              "type": "char_string",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": null,
              "defaultValue": "0",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "sw build id",
              "code": 16384,
              "mfgCode": null,
              "side": "server",
              "type": "char_string",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": 0,
              "defaultValue": "v00.00.02",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "cluster revision",
              "code": 65533,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": null,
              "defaultValue": "3",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            }
          ]
        },
        {
          "name": "Identify",
          "code": 3,
          "mfgCode": null,
          "define": "IDENTIFY_CLUSTER",
          "side": "client",
          "enabled": 1,
          "commands": [
            {
              "name": "Identify",
              "code": 0,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "IdentifyQueryResponse",
              "code": 0,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "IdentifyQuery",
              "code": 1,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 0,
              "isEnabled": 1
            }
          ],
          "attributes": [
            {
              "name": "cluster revision",
              "code": 65533,
              "mfgCode": null,
              "side": "client",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": null,
              "defaultValue": "2",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            }
          ]
        },
        {
          "name": "Identify",
          "code": 3,
          "mfgCode": null,
          "define": "IDENTIFY_CLUSTER",
          "side": "server",
          "enabled": 1,
          "commands": [
            {
              "name": "Identify",
              "code": 0,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "IdentifyQueryResponse",
              "code": 0,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "IdentifyQuery",
              "code": 1,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            }
          ],
          "attributes": [
            {
              "name": "identify time",
              "code": 0,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": null,
              "defaultValue": "60",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "cluster revision",
              "code": 65533,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": null,
              "defaultValue": "2",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            }
          ]
        },
        {
          "name": "Groups",
          "code": 4,
          "mfgCode": null,
          "define": "GROUPS_CLUSTER",
          "side": "client",
          "enabled": 1,
          "commands": [
            {
              "name": "AddGroup",
              "code": 0,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "AddGroupResponse",
              "code": 0,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "ViewGroup",
              "code": 1,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "ViewGroupResponse",
              "code": 1,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "GetGroupMembershipResponse",
              "code": 2,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "GetGroupMembership",
              "code": 2,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "RemoveGroup",
              "code": 3,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "RemoveGroupResponse",
              "code": 3,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "RemoveAllGroups",
              "code": 4,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "AddGroupIfIdentifying",
              "code": 5,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 0,
              "isEnabled": 1
            }
          ],
          "attributes": [
            {
              "name": "cluster revision",
              "code": 65533,
              "mfgCode": null,
              "side": "client",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": null,
              "defaultValue": "3",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            }
          ]
        },
        {
          "name": "Groups",
          "code": 4,
          "mfgCode": null,
          "define": "GROUPS_CLUSTER",
          "side": "server",
          "enabled": 1,
          "commands": [
            {
              "name": "AddGroupResponse",
              "code": 0,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "AddGroup",
              "code": 0,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "ViewGroup",
              "code": 1,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "ViewGroupResponse",
              "code": 1,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "GetGroupMembership",
              "code": 2,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "GetGroupMembershipResponse",
              "code": 2,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "RemoveGroup",
              "code": 3,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "RemoveGroupResponse",
              "code": 3,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "RemoveAllGroups",
              "code": 4,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "AddGroupIfIdentifying",
              "code": 5,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            }
          ],
          "attributes": [
            {
              "name": "name support",
              "code": 0,
              "mfgCode": null,
              "side": "server",
              "type": "bitmap8",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": null,
              "defaultValue": "128",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "cluster revision",
              "code": 65533,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": null,
              "defaultValue": "3",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            }
          ]
        },
        {
          "name": "Occupancy Sensing",
          "code": 1030,
          "mfgCode": null,
          "define": "OCCUPANCY_SENSING_CLUSTER",
          "side": "server",
          "enabled": 1,
          "attributes": [
            {
              "name": "occupancy",
              "code": 0,
              "mfgCode": null,
              "side": "server",
              "type": "bitmap8",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "occupancy sensor type",
              "code": 1,
              "mfgCode": null,
              "side": "server",
              "type": "enum8",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x00",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "occupancy sensor type bitmap",
              "code": 2,
              "mfgCode": null,
              "side": "server",
              "type": "bitmap8",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x00",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "PIR occupied to unoccupied delay",
              "code": 16,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "NVM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x0E10",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "PIR unoccupied to occupied delay",
              "code": 17,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "NVM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x0000",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "PIR unoccupied to occupied threshold",
              "code": 18,
              "mfgCode": null,
              "side": "server",
              "type": "int8u",
              "included": 1,
              "storageOption": "NVM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x01",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "cluster revision",
              "code": 65533,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "2",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            }
          ]
        }
      ]
    },
    {
      "id": 3,
      "name": "Anonymous Endpoint Type",
      "deviceTypeRef": {
        "code": 262,
        "profileId": 260,
        "label": "LO-lightsensor",
        "name": "LO-lightsensor",
        "deviceTypeOrder": 0
      },
      "deviceTypes": [
        {
          "code": 262,
          "profileId": 260,
          "label": "LO-lightsensor",
          "name": "LO-lightsensor",
          "deviceTypeOrder": 0
        }
      ],
      "deviceVersions": [
        1
      ],
      "deviceIdentifiers": [
        262
      ],
      "deviceTypeName": "LO-lightsensor",
      "deviceTypeCode": 262,
      "deviceTypeProfileId": 260,
      "clusters": [
        {
          "name": "Basic",
          "code": 0,
          "mfgCode": null,
          "define": "BASIC_CLUSTER",
          "side": "server",
          "enabled": 1,
          "attributes": [
            {
              "name": "ZCL version",
              "code": 0,
              "mfgCode": null,
              "side": "server",
              "type": "int8u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": null,
              "defaultValue": "8",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "application version",
              "code": 1,
              "mfgCode": null,
              "side": "server",
              "type": "int8u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": null,
              "defaultValue": "0",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "stack version",
              "code": 2,
              "mfgCode": null,
              "side": "server",
              "type": "int8u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": null,
              "defaultValue": "82",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "hardware version",
              "code": 3,
              "mfgCode": null,
              "side": "server",
              "type": "int8u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": null,
              "defaultValue": "1",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "manufacturer name",
              "code": 4,
              "mfgCode": null,
              "side": "server",
              "type": "char_string",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": 0,
              "defaultValue": "Adaptiv Ltd",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "model identifier",
              "code": 5,
              "mfgCode": null,
              "side": "server",
              "type": "char_string",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": 0,
              "defaultValue": "NT-3",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "date code",
              "code": 6,
              "mfgCode": null,
              "side": "server",
              "type": "char_string",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": null,
              "defaultValue": "20250101",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "power source",
              "code": 7,
              "mfgCode": null,
              "side": "server",
              "type": "enum8",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": null,
              "defaultValue": "1",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "generic device class",
              "code": 8,
              "mfgCode": null,
              "side": "server",
              "type": "enum8",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": null,
              "defaultValue": "0",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "generic device type",
              "code": 9,
              "mfgCode": null,
              "side": "server",
              "type": "enum8",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": null,
              "defaultValue": "9",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "product code",
              "code": 10,
              "mfgCode": null,
              "side": "server",
              "type": "octet_string",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": null,
              "defaultValue": "NT-3.0",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "product url",
              "code": 11,
              "mfgCode": null,
              "side": "server",
              "type": "char_string",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": null,
              "defaultValue": "0",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "sw build id",
              "code": 16384,
              "mfgCode": null,
              "side": "server",
              "type": "char_string",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": 0,
              "defaultValue": "v00.00.02",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "cluster revision",
              "code": 65533,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": null,
              "defaultValue": "3",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            }
          ]
        },
        {
          "name": "Identify",
          "code": 3,
          "mfgCode": null,
          "define": "IDENTIFY_CLUSTER",
          "side": "client",
          "enabled": 1,
          "commands": [
            {
              "name": "Identify",
              "code": 0,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "IdentifyQueryResponse",
              "code": 0,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "IdentifyQuery",
              "code": 1,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 0,
              "isEnabled": 1
            }
          ],
          "attributes": [
            {
              "name": "cluster revision",
              "code": 65533,
              "mfgCode": null,
              "side": "client",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": null,
              "defaultValue": "2",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            }
          ]
        },
        {
          "name": "Identify",
          "code": 3,
          "mfgCode": null,
          "define": "IDENTIFY_CLUSTER",
          "side": "server",
          "enabled": 1,
          "commands": [
            {
              "name": "Identify",
              "code": 0,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "IdentifyQueryResponse",
              "code": 0,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "IdentifyQuery",
              "code": 1,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 1,
              "isEnabled": 1
            }
          ],
          "attributes": [
            {
              "name": "identify time",
              "code": 0,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": null,
              "defaultValue": "60",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "cluster revision",
              "code": 65533,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 1,
              "bounded": null,
              "defaultValue": "2",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            }
          ]
        },
        {
          "name": "Groups",
          "code": 4,
          "mfgCode": null,
          "define": "GROUPS_CLUSTER",
          "side": "client",
          "enabled": 1,
          "commands": [
            {
              "name": "AddGroup",
              "code": 0,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "AddGroupResponse",
              "code": 0,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "ViewGroup",
              "code": 1,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "ViewGroupResponse",
              "code": 1,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "GetGroupMembershipResponse",
              "code": 2,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "GetGroupMembership",
              "code": 2,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "RemoveGroup",
              "code": 3,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "RemoveGroupResponse",
              "code": 3,
              "mfgCode": null,
              "source": "server",
              "isIncoming": 1,
              "isEnabled": 1
            },
            {
              "name": "RemoveAllGroups",
              "code": 4,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 0,
              "isEnabled": 1
            },
            {
              "name": "AddGroupIfIdentifying",
              "code": 5,
              "mfgCode": null,
              "source": "client",
              "isIncoming": 0,
              "isEnabled": 1
            }
          ],
          "attributes": [
            {
              "name": "cluster revision",
              "code": 65533,
              "mfgCode": null,
              "side": "client",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": null,
              "defaultValue": "3",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            }
          ]
        },
        {
          "name": "Groups",
          "code": 4,
          "mfgCode": null,
          "define": "GROUPS_CLUSTER",
          "side": "server",
          "enabled": 1,
          "attributes": [
            {
              "name": "name support",
              "code": 0,
              "mfgCode": null,
              "side": "server",
              "type": "bitmap8",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": null,
              "defaultValue": "128",
              "reportable": 0,
              "minInterval": "0x00",
              "maxInterval": "0x80",
              "reportableChange": 0
            },
            {
              "name": "cluster revision",
              "code": 65533,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": null,
              "defaultValue": "3",
              "reportable": 0,
              "minInterval": "0x0001",
              "maxInterval": "0xFFFE",
              "reportableChange": 0
            }
          ]
        },
        {
          "name": "Illuminance Measurement",
          "code": 1024,
          "mfgCode": null,
          "define": "ILLUM_MEASUREMENT_CLUSTER",
          "side": "server",
          "enabled": 1,
          "attributes": [
            {
              "name": "measured value",
              "code": 0,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x0000",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "min measured value",
              "code": 1,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0x0001",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "max measured value",
              "code": 2,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "0xFFFE",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            },
            {
              "name": "cluster revision",
              "code": 65533,
              "mfgCode": null,
              "side": "server",
              "type": "int16u",
              "included": 1,
              "storageOption": "RAM",
              "singleton": 0,
              "bounded": 0,
              "defaultValue": "2",
              "reportable": 0,
              "minInterval": 1,
              "maxInterval": 65534,
              "reportableChange": 0
            }
          ]
        }
      ]
    }
  ],
  "endpoints": [
    {
      "endpointTypeName": "Anonymous Endpoint Type",
      "endpointTypeIndex": 0,
      "profileId": 260,
      "endpointId": 1,
      "networkId": 0,
      "parentEndpointIdentifier": null
    },
    {
      "endpointTypeName": "Anonymous Endpoint Type",
      "endpointTypeIndex": 1,
      "profileId": 260,
      "endpointId": 2,
      "networkId": 0,
      "parentEndpointIdentifier": null
    },
    {
      "endpointTypeName": "Anonymous Endpoint Type",
      "endpointTypeIndex": 2,
      "profileId": 260,
      "endpointId": 3,
      "networkId": 0,
      "parentEndpointIdentifier": null
    }
  ]
}
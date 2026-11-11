-- UAL1_Standard 스켈레탈 메시 액터의 ScriptPath에 연결한 뒤 PIE에서 사용한다.
-- 이동 컴포넌트가 없는 액터에서는 W 걷기, Shift+W 달리기를 제자리에서 미리 본다.
local pack = "Assets/Models/UAL1/UAL1_Standard.fbx"
local walkSpeed = 1.0
local sprintSpeed = 3.0
local previewSpeed
local useMovementVelocity
local ready = false
local attackEnabled = true -- false로 바꾸면 Lua 전이 규칙이 공격을 막는다.
local airTime = 1.8
local jumpHeight = 1.0
local jumpElapsed

local function ResetJump()
    if script_state.jumpGroundZ ~= nil then
        local position = obj.Location
        position.z = script_state.jumpGroundZ
        obj.Location = position
        script_state.jumpGroundZ = nil
    end
    jumpElapsed = nil
end

local function Setup()
    ready = obj:ConfigureCharacterAnimations(
        pack, pack, pack,
        "Armature|Idle_Loop", "Armature|Sword_Attack", "Armature|Hit_Chest",
        {
            path = pack,
            start = "Armature|Jump_Start",
            loop = "Armature|Jump_Loop",
            land = "Armature|Jump_Land"
        },
        {
            { take = "Idle_Loop", x = 0 },
            { take = "Walk_Loop", x = walkSpeed },
            { take = "Sprint_Loop", x = sprintSpeed }
        }
    )
    previewSpeed = 0
    useMovementVelocity = obj:HasComponent("UMovementComponent")
    ResetJump()
    obj:SetAnimAttackRule(function(requested)
        return requested and attackEnabled
    end)
    obj:SetRestartHitOnReentry(script_state.restartHit == true)
end

local function UpdateJump(dt)
    if jumpElapsed == nil then
        if not IsKeyPressed("space") or not obj:RequestAnimJump() then return end
        script_state.jumpGroundZ = obj.Location.z
        jumpElapsed = 0
    end

    jumpElapsed = math.min(jumpElapsed + dt, airTime)
    local t = jumpElapsed / airTime
    local position = obj.Location
    position.z = script_state.jumpGroundZ + 4 * jumpHeight * t * (1 - t)
    obj.Location = position

    if t >= 1 then
        script_state.jumpGroundZ = nil
        jumpElapsed = nil
        obj:NotifyAnimLanded()
    end
end

function BeginPlay()
    Setup()
end

function OnReload()
    Setup()
end

local function UpdateLocomotion(dt)
    if useMovementVelocity then
        local velocity = obj.Velocity
        obj:SetAnimSpeed(math.sqrt(velocity.x * velocity.x + velocity.y * velocity.y))
    else
        local target = IsKeyDown("W") and (IsKeyDown("shift") and sprintSpeed or walkSpeed) or 0
        local step = sprintSpeed * dt
        previewSpeed = previewSpeed + math.max(-step, math.min(step, target - previewSpeed))
        obj:SetAnimSpeed(previewSpeed)
    end
end

function Tick(dt)
    if not ready then return end

    UpdateLocomotion(dt)
    if IsKeyPressed("A") then
        obj:RequestAnimAttack()
    end
    if IsKeyPressed("H") then
        obj:RequestAnimHit()
    end
    if IsKeyPressed("R") then
        script_state.restartHit = not script_state.restartHit
        obj:SetRestartHitOnReentry(script_state.restartHit)
    end
    UpdateJump(dt)
end

function EndPlay()
    ResetJump()
end

function OnAnimNotify(notifyName, clipName)
    print("[Lua AnimNotify] '" .. tostring(notifyName) .. "' from clip '" .. tostring(clipName) .. "'")
end

function OnAnimNotifyBegin(notifyName, clipName, duration)
    print(string.format("[Lua AnimNotifyState] Begin '%s' from clip '%s' (Duration: %.3fs)",
        tostring(notifyName), tostring(clipName), tonumber(duration) or 0))
end

-- function OnAnimNotifyTick(notifyName, clipName, dt)
-- end

function OnAnimNotifyEnd(notifyName, clipName)
    print(string.format("[Lua AnimNotifyState] End '%s' from clip '%s'",
        tostring(notifyName), tostring(clipName)))
end

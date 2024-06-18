#include <cstring>
#include <iostream>
#include <vector>

#include <steam/steam_api.h>

#include <raylib/raylib.h>

#define RAYGUI_IMPLEMENTATION
#include <raylib/raygui.h>

#define APP_ID 480

#define LOG( x ) std::cout << ( x ) << std::endl;
#define MIN( x, y ) ((x) < (y) ? (x) : (y))

enum eScreenState {
    LOADING = 0,
    OUTSIDE_LOBBY,
    LOBBY_CREATION,
    LOBBY_JOIN,
    LOBBY
} screen_state;

static struct {
    bool should_quit = false;
    std::string loading_screen_text;
} program;

class Screen {
public:
    static void renderOutsideLobby();
    static void renderLobbyCreation();
    static void renderLobbyJoin();
    static void renderLobby();
    static void renderLoading();
};

class LobbyManager {
public:
    char lobby_name[100];
    std::string lobby_leader;
    std::vector<uint64> members;
    uint64 id;

    void CreateLobby();
    void JoinLobby(uint64 SteamID);
    void LeaveLobby();

private:
    // call Values
    // where u handle the lobby creation. Till this is called, we show a Loading Screen
    void OnLobbyCreate( LobbyCreated_t *pCallback, bool bIOFailure );
    CCallResult< LobbyManager, LobbyCreated_t > m_LobbyCreateCallResult;

    void OnLobbyJoin( LobbyEnter_t *pCallback, bool bIOFailure );
    CCallResult< LobbyManager, LobbyEnter_t > m_LobbyJoinCallResult;

    // Call Backs
    STEAM_CALLBACK( LobbyManager, OnLobbyDataUpdate, LobbyDataUpdate_t );
};

static LobbyManager lobby_manager;

int main() {
    // This Starts the game in Steam
    if ( SteamAPI_RestartAppIfNecessary( APP_ID ) ) {
        return 1;
    }

    InitWindow(800, 600, "Steam Test");
    SetWindowState(FLAG_WINDOW_RESIZABLE);

    SetExitKey(0);

    if ( !SteamAPI_Init() ) {
        std::cout << "An instance of Steam needs to be running" << std::endl;
        return EXIT_FAILURE;
    }

    screen_state = eScreenState::OUTSIDE_LOBBY;
    SetTargetFPS(100);

    while ( !WindowShouldClose() && !program.should_quit ) {
        BeginDrawing();
        DrawFPS(0, 0);
        SteamAPI_RunCallbacks();
        ClearBackground(RAYWHITE);
        // LOG( screen_state );
        switch ( screen_state ) {
            case eScreenState::LOADING:
                Screen::renderLoading();
                break;
            case eScreenState::OUTSIDE_LOBBY:
                Screen::renderOutsideLobby();
                break;
            case eScreenState::LOBBY_CREATION:
                Screen::renderLobbyCreation();
                break;
            case eScreenState::LOBBY_JOIN:
                Screen::renderLobbyJoin();
                break;
            case eScreenState::LOBBY:
                Screen::renderLobby();
                break;
        }

        EndDrawing();
    }
    lobby_manager.LeaveLobby();
    CloseWindow();
    SteamAPI_Shutdown();
}

void Screen::renderLoading() {
    DrawText(program.loading_screen_text.c_str(), 100, 100, 32, BLACK);
}

void Screen::renderOutsideLobby() {
    if ( GuiButton( (Rectangle) {0, 0, 300, 50}, "Create Lobby" ) ) {
        screen_state = eScreenState::LOBBY_CREATION;
    }
    if ( GuiButton( (Rectangle) {0, 100, 300, 50}, "Join Lobby" ) ) {
        screen_state = eScreenState::LOBBY_JOIN;
    }
    if ( GuiButton( (Rectangle) {0, 200, 300, 50}, "Quit" ) ) {
        program.should_quit = true;
    }
}

void Screen::renderLobbyCreation() {
    float window_width = GetScreenWidth();
    float window_height = GetScreenHeight();

    float w = 0.3 * window_width;
    float h = MIN( 0.1 * window_height, 30 );

    Rectangle textbox_bounds = { 
        static_cast<float>(0.5 * (window_width - w)),
        static_cast<float>(0.5 * (window_height - h)), 
        w, h
    };

    Rectangle button_bounds = {
        static_cast<float>(0.5 * (window_width - w)),
        static_cast<float>(0.5 * (window_height - h)) + 100, 
        w, h
    };

    if ( GuiTextBox( textbox_bounds, lobby_manager.lobby_name, 100, true ) || GuiButton( button_bounds, "Create Lobby" )) {
        if ( strlen( lobby_manager.lobby_name ) > 6 ) {
            screen_state = eScreenState::LOADING;
            program.loading_screen_text = "Creating Lobby";

            lobby_manager.CreateLobby();
        }
    }
}

void Screen::renderLobbyJoin() {

}

void Screen::renderLobby() {
    // DrawText( TextFormat( "Lobby ID: %lld", lobby_manager.id ), 100, 100, 32, BLACK );
    // DrawText( TextFormat( "Lobby Leader: %s", lobby_manager.lobby_leader.c_str() ), 100, 200, 32, BLACK );
    // DrawText( TextFormat( "Lobby Name: %s", lobby_manager.lobby_name ), 100, 300, 32, BLACK );
    // DrawText( "Copied Lobby ID to Clip board", 100, GetScreenHeight() - 100, 20, GRAY );
    Rectangle members_panel = {
        0, 0, 
        static_cast<float>( 0.2 * GetScreenWidth() ), 
        static_cast<float>( GetScreenHeight() )
    };

    Rectangle chat_panel = {
        members_panel.width, 0,
        static_cast<float>( GetScreenWidth() - members_panel.width ),
        members_panel.height
    };

    GuiPanel( members_panel, TextFormat( "Online: %d", lobby_manager.members.size() ) );
    GuiPanel( chat_panel, TextFormat( "%s Lobby: Owned by %s", lobby_manager.lobby_name, lobby_manager.lobby_leader.c_str()) );
    
    float font_height = 20;
    Vector2 padding = {10, 50};
    for (int i = 0; i < lobby_manager.members.size(); i++) {
        float y_offset = i * font_height;

        const char* username = SteamFriends()->GetFriendPersonaName( lobby_manager.members[i] );
        DrawText(username, padding.x, padding.y + y_offset, font_height / 10, BLACK);
    }

    SetClipboardText( TextFormat( "%lld", lobby_manager.id ) );
}

// Lobby Manager Implementation

void LobbyManager::CreateLobby() {
    SteamAPICall_t hSteamAPICall = SteamMatchmaking()->CreateLobby( k_ELobbyTypePublic, 100 );
    m_LobbyCreateCallResult.Set( hSteamAPICall, this, &LobbyManager::OnLobbyCreate );
}

void LobbyManager::OnLobbyCreate( LobbyCreated_t *pCallback, bool bIOFailure ) {

    if ( bIOFailure ) {
        LOG( "[INTERNAL ERROR] Couldn't Create Server" );
    }

    bool failed = true;
    switch ( pCallback->m_eResult ) {
        case k_EResultFail:
            TraceLog( LOG_ERROR, "The server responded, but with an unknown internal error." );
            break;
        case k_EResultTimeout:
            TraceLog( LOG_ERROR, "The message was sent to the Steam servers, but it didn't respond." );
            break;
        case k_EResultLimitExceeded:
            TraceLog( LOG_ERROR, "Your game client has created too many lobbies and is being rate limited." );
            break;
        case k_EResultAccessDenied:
            TraceLog( LOG_ERROR, "Your game isn't set to allow lobbies, or your client does haven't rights to play the game" );
            break;
        case k_EResultNoConnection:
            TraceLog( LOG_ERROR, "Your Steam client doesn't have a connection to the back-end." );
            break;
        case k_EResultOK:
            TraceLog(LOG_INFO, "Created Lobby Succesfully. Lobby ID: %lld", pCallback->m_ulSteamIDLobby);
            failed = false;
            break;
        default:
            TraceLog(LOG_ERROR, "Shouldn't be reachable");
            return;
    }

    if ( failed ) {
        screen_state = eScreenState::OUTSIDE_LOBBY;
        return;
    }

    lobby_manager.id = pCallback->m_ulSteamIDLobby;
    if ( !SteamMatchmaking()->SetLobbyData( (CSteamID) lobby_manager.id, "lobby_name", std::string( lobby_manager.lobby_name ).c_str() ) ) {
        TraceLog(LOG_ERROR, "Invalid Lobby ID");
    }

    // First member in the lobby
    lobby_manager.members.push_back( SteamUser()->GetSteamID().ConvertToUint64() );
    lobby_manager.lobby_leader = SteamFriends()->GetPersonaName();

    if ( !SteamMatchmaking()->SetLobbyData( lobby_manager.id, "lobby_leader", lobby_manager.lobby_leader.c_str() ) ) {
        TraceLog(LOG_ERROR, "Invalid Lobby ID");
    }

    screen_state = eScreenState::LOBBY;
}

void LobbyManager::JoinLobby(uint64 SteamID) {
    SteamAPICall_t hSteamAPICall = SteamMatchmaking()->JoinLobby(SteamID);
    m_LobbyJoinCallResult.Set( hSteamAPICall, this, &LobbyManager::OnLobbyJoin );
}

void LobbyManager::OnLobbyJoin( LobbyEnter_t *pCallback, bool bIOFailure ) {
    if ( bIOFailure || pCallback->m_EChatRoomEnterResponse == k_EChatRoomEnterResponseError ) {
        TraceLog(LOG_ERROR, "Coudln't Join Lobby");
        return;
    }

    lobby_manager.id = pCallback->m_ulSteamIDLobby;
    int nMembers = SteamMatchmaking()->GetNumLobbyMembers( lobby_manager.id );
    for (int i = 0; i < nMembers; i++) {
        lobby_manager.members.push_back( SteamMatchmaking()->GetLobbyMemberByIndex( lobby_manager.id, i ).ConvertToUint64() );
    }
    lobby_manager.lobby_leader = SteamMatchmaking()->GetLobbyData( lobby_manager.id, "l0bby_leader" );
    strncpy( lobby_manager.lobby_name, SteamMatchmaking()->GetLobbyData( lobby_manager.id, "lobby_leader" ), 100 );

    TraceLog(LOG_INFO, "Joined Lobby %lld", lobby_manager.id);
    screen_state = eScreenState::LOBBY;
}

void LobbyManager::LeaveLobby() {
    if (id == 0) {
        TraceLog(LOG_WARNING, "Not Connected to Any Lobby");
        return;
    }
    SteamMatchmaking()->LeaveLobby( this->id );
    this->id = 0;
    this->lobby_leader.clear();
    this->members.clear();
}

// Call Backs

void LobbyManager::OnLobbyDataUpdate( LobbyDataUpdate_t *pCallback ) {
    uint64 lobby_id = pCallback->m_ulSteamIDLobby;
    uint64 member_id = pCallback->m_ulSteamIDMember;

    if (lobby_id == member_id) {
        // Lobby Changed
        int nMembers = SteamMatchmaking()->GetNumLobbyMembers( lobby_id );
        if (nMembers != lobby_manager.members.size()) {
            // User joined or left
        }
        return;
    }
    
    // User changed
}

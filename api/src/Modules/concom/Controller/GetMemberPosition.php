<?php declare(strict_types=1);
/*.
    require_module 'standard';
.*/

namespace App\Modules\concom\Controller;

use Slim\Http\Request;
use Slim\Http\Response;
use App\Controller\PermissionDeniedException;
use App\Controller\NotFoundException;

require_once __DIR__.'/../../../../../modules/concom/functions/POSITION.inc';

class GetMemberPosition extends BaseConcom
{


    public function buildResource(Request $request, Response $response, $args): array
    {
        $user = $request->getAttribute('oauth2-token')['user_id'];
        if (array_key_exists('id', $args)) {
            $data = \lookup_users_by_key($args['id']);
            if (empty($data['users'])) {
                if (empty($data['error'])) {
                    $error = 'No Members Found';
                } else {
                    $error = $data['error'];
                }
                throw new NotFoundException($error);
            }
            if ($data[0]['Id'] != $user &&
                !\ciab\RBAC::havePermission('api.get.concom')) {
                throw new PermissionDeniedException();
            }
            $user = $data['users'][0]['Id'];
        }
        $concom = \concom\POSITION::getConComPosition($user);
        $data = [];
        $path = $request->getUri()->getBaseUrl();
        foreach ($concom as $entry) {
            $data[] = $this->buildEntry($request, $entry['departmentId'], $user, $entry['note'], $entry['position']);
        }
        return [
        \App\Controller\BaseController::LIST_TYPE,
        $data,
        array('type' => 'concom_list')
        ];

    }


    /* end GetMemberPosition */
}

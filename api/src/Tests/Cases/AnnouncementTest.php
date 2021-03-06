<?php

namespace App\Tests\TestCase\Controller;

use App\Tests\Base\CiabTestCase;

class AnnouncementTest extends CiabTestCase
{

    private $aid = array();


    protected function addAnnouncement($scope): string
    {
        $data = $this->runSuccessJsonRequest('GET', '/department/1/announcements');
        $initial_ids = [];
        foreach ($data->data as $item) {
            $initial_ids[] = $item->id;
        }

        $this->runSuccessRequest(
            'POST',
            '/department/1/announcement',
            null,
            ['Scope' => $scope,
             'Text' => 'testing',
             'Email' => 1],
            201
        );

        $data = $this->runSuccessJsonRequest('GET', '/department/1/announcements');

        # Find New Index
        $target = null;
        foreach ($data->data as $item) {
            if ($target == null && !in_array($item->id, $initial_ids)) {
                $target = $item->id;
                $this->assertSame([
                    'type' => 'announcement',
                    'id' => $target,
                    'departmentId' => '1',
                    'postedOn' => $item->postedOn,
                    'postedBy' => '1000',
                    'scope' => "$scope",
                    'text' => 'testing'
                ], (array)$item);
            }
        }

        $data = $this->runSuccessJsonRequest(
            'GET',
            '/announcement/'.$target,
            ['fields' => 'type,id,departmentId,postedBy,scope,text']
        );
        $this->assertSame([
            'type' => 'announcement',
            'id' => $target,
            'departmentId' => '1',
            'postedBy' => '1000',
            'scope' => "$scope",
            'text' => 'testing'
        ], (array)$data);

        return $target;

    }


    protected function setUp(): void
    {
        parent::setUp();
        for ($i = 0; $i < 4; $i ++) {
            $this->aid[$i] = $this->addAnnouncement($i);
        }

    }


    protected function tearDown(): void
    {
        parent::tearDown();
        foreach ($this->aid as $target) {
            $this->runSuccessRequest('DELETE', '/announcement/'.$target, null, null, 204);
        }

    }


    public function testAnnounce(): void
    {
        $data = $this->runSuccessJsonRequest(
            'GET',
            '/department/1/announcements',
            ['include' => 'departmentId,postedBy']
        );
        $this->assertNotEmpty($data->data);

        $this->runSuccessRequest(
            'PUT',
            '/announcement/'.$this->aid[1],
            null,
            ['Department' => 2,
             'Text' => 'New Message',
             'Scope' => 1]
        );

        $data = $this->runSuccessJsonRequest(
            'GET',
            '/announcement/'.$this->aid[1],
            ['include' => 'departmentId,postedBy']
        );
        $this->assertSame($data->text, 'New Message');

        $data = $this->runSuccessJsonRequest(
            'GET',
            '/member/1000/announcements',
            ['include' => 'departmentId,postedBy']
        );
        $this->assertNotEmpty($data->data);

    }


    public function testAnnounceErrors(): void
    {
        $this->runRequest(
            'GET',
            '/department/-1/announcements',
            null,
            null,
            404
        );

        $this->runRequest('PUT', '/announcement/'.$this->aid[2], null, null, 400);

        $this->runRequest('PUT', '/announcement/-1', null, null, 404);

        $this->runRequest(
            'PUT',
            '/announcement/'.$this->aid[2],
            null,
            ['Department' => -1],
            400
        );

        $this->runRequest('POST', '/department/1/announcement', null, null, 400);

        $this->runRequest(
            'POST',
            '/department/-1/announcement',
            null,
            ['Scope' => 2,
             'Text' => 'testing',
             'Email' => 0],
            404
        );

        $this->runRequest(
            'POST',
            '/department/1/announcement',
            null,
            ['Text' => 'testing',
             'Email' => 0],
            400
        );

        $this->runRequest(
            'POST',
            '/department/1/announcement',
            null,
            ['Scope' => 2,
             'Email' => 0],
            400
        );

    }


    /* End */
}
